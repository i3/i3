/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * manage.c: Contains all functions for initially managing new windows
 *           (or existing ones on restart).
 *
 */

#include "all.h"

/*
 * Go through all existing windows (if the window manager is restarted) and manage them
 *
 */
void manage_existing_windows(xcb_window_t root) {
    xcb_query_tree_reply_t *reply;
    int i, len;
    xcb_window_t *children;
    xcb_get_window_attributes_cookie_t *cookies;

    /* Get the tree of windows whose parent is the root window (= all) */
    if ((reply = xcb_query_tree_reply(conn, xcb_query_tree(conn, root), 0)) == NULL)
        return;

    len = xcb_query_tree_children_length(reply);
    cookies = smalloc(len * sizeof(*cookies));

    /* Request the window attributes for every window */
    children = xcb_query_tree_children(reply);
    for (i = 0; i < len; ++i)
        cookies[i] = xcb_get_window_attributes(conn, children[i]);

    /* Call manage_window with the attributes for every window */
    for (i = 0; i < len; ++i)
        manage_window(children[i], cookies[i], true);

    free(reply);
    free(cookies);
}

/*
 * Restores the geometry of each window by reparenting it to the root window
 * at the position of its frame.
 *
 * This is to be called *only* before exiting/restarting i3 because of evil
 * side-effects which are to be expected when continuing to run i3.
 *
 */
void restore_geometry() {
    LOG("Restoring geometry\n");

    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons)
        if (con->window) {
            printf("placing window at %d %d\n", con->rect.x, con->rect.y);
            xcb_reparent_window(conn, con->window->id, root,
                                con->rect.x, con->rect.y);
        }

    /* Make sure our changes reach the X server, we restart/exit now */
    xcb_flush(conn);
}

/*
 * Do some sanity checks and then reparent the window.
 *
 */
void manage_window(xcb_window_t window, xcb_get_window_attributes_cookie_t cookie,
                   bool needs_to_be_mapped) {
    xcb_drawable_t d = { window };
    xcb_get_geometry_cookie_t geomc;
    xcb_get_geometry_reply_t *geom;
    xcb_get_window_attributes_reply_t *attr = 0;

    printf("---> looking at window 0x%08x\n", window);

    xcb_get_property_cookie_t wm_type_cookie, strut_cookie, state_cookie,
                              utf8_title_cookie, title_cookie,
                              class_cookie, leader_cookie, transient_cookie;

    wm_type_cookie = xcb_get_any_property_unchecked(conn, false, window, atoms[_NET_WM_WINDOW_TYPE], UINT32_MAX);
    strut_cookie = xcb_get_any_property_unchecked(conn, false, window, atoms[_NET_WM_STRUT_PARTIAL], UINT32_MAX);
    state_cookie = xcb_get_any_property_unchecked(conn, false, window, atoms[_NET_WM_STATE], UINT32_MAX);
    utf8_title_cookie = xcb_get_any_property_unchecked(conn, false, window, atoms[_NET_WM_NAME], 128);
    leader_cookie = xcb_get_any_property_unchecked(conn, false, window, atoms[WM_CLIENT_LEADER], UINT32_MAX);
    transient_cookie = xcb_get_any_property_unchecked(conn, false, window, WM_TRANSIENT_FOR, UINT32_MAX);
    title_cookie = xcb_get_any_property_unchecked(conn, false, window, WM_NAME, 128);
    class_cookie = xcb_get_any_property_unchecked(conn, false, window, WM_CLASS, 128);
    /* TODO: also get wm_normal_hints here. implement after we got rid of xcb-event */

    geomc = xcb_get_geometry(conn, d);

    /* Check if the window is mapped (it could be not mapped when intializing and
       calling manage_window() for every window) */
    if ((attr = xcb_get_window_attributes_reply(conn, cookie, 0)) == NULL) {
        LOG("Could not get attributes\n");
        return;
    }

    if (needs_to_be_mapped && attr->map_state != XCB_MAP_STATE_VIEWABLE) {
        LOG("map_state unviewable\n");
        goto out;
    }

    /* Don’t manage clients with the override_redirect flag */
    LOG("override_redirect is %d\n", attr->override_redirect);
    if (attr->override_redirect)
        goto out;

    /* Check if the window is already managed */
    if (con_by_window_id(window) != NULL) {
        LOG("already managed (by con %p)\n", con_by_window_id(window));
        goto out;
    }

    /* Get the initial geometry (position, size, …) */
    if ((geom = xcb_get_geometry_reply(conn, geomc, 0)) == NULL) {
        LOG("could not get geometry\n");
        goto out;
    }

    LOG("reparenting!\n");
    uint32_t mask = 0;
    uint32_t values[1];

    i3Window *cwindow = scalloc(sizeof(i3Window));
    cwindow->id = window;

    /* We need to grab the mouse buttons for click to focus */
    xcb_grab_button(conn, false, window, XCB_EVENT_MASK_BUTTON_PRESS,
                    XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                    1 /* left mouse button */,
                    XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

    xcb_grab_button(conn, false, window, XCB_EVENT_MASK_BUTTON_PRESS,
                    XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                    3 /* right mouse button */,
                    XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);


    /* update as much information as possible so far (some replies may be NULL) */
    window_update_class(cwindow, xcb_get_property_reply(conn, class_cookie, NULL));
    window_update_name_legacy(cwindow, xcb_get_property_reply(conn, title_cookie, NULL));
    window_update_name(cwindow, xcb_get_property_reply(conn, utf8_title_cookie, NULL));
    window_update_leader(cwindow, xcb_get_property_reply(conn, leader_cookie, NULL));
    window_update_transient_for(cwindow, xcb_get_property_reply(conn, transient_cookie, NULL));

    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, wm_type_cookie, NULL);
    if (xcb_reply_contains_atom(reply, atoms[_NET_WM_WINDOW_TYPE_DOCK])) {
        cwindow->dock = true;
        LOG("this window is a dock\n");
    }


    Con *nc;
    Match *match;

    /* TODO: assignments */
    /* TODO: two matches for one container */
    /* See if any container swallows this new window */
    nc = con_for_window(cwindow, &match);
    if (nc == NULL) {
        if (focused->type == CT_CON && con_accepts_window(focused)) {
            LOG("using current container, focused = %p, focused->name = %s\n",
                            focused, focused->name);
            nc = focused;
        } else nc = tree_open_con(NULL);
    } else {
        if (match != NULL && match->insert_where == M_ACTIVE) {
            /* We need to go down the focus stack starting from nc */
            while (TAILQ_FIRST(&(nc->focus_head)) != TAILQ_END(&(nc->focus_head))) {
                printf("walking down one step...\n");
                nc = TAILQ_FIRST(&(nc->focus_head));
            }
            /* We need to open a new con */
            /* TODO: make a difference between match-once containers (directly assign
             * cwindow) and match-multiple (tree_open_con first) */
            nc = tree_open_con(nc->parent);
        }
    }
    DLOG("new container = %p\n", nc);
    nc->window = cwindow;
    x_reinit(nc);

    nc->border_width = geom->border_width;

    char *name;
    asprintf(&name, "[i3 con] container around %p", cwindow);
    x_set_name(nc, name);
    free(name);

    /* set floating if necessary */
    bool want_floating = false;
    if (xcb_reply_contains_atom(reply, atoms[_NET_WM_WINDOW_TYPE_DIALOG]) ||
        xcb_reply_contains_atom(reply, atoms[_NET_WM_WINDOW_TYPE_UTILITY]) ||
        xcb_reply_contains_atom(reply, atoms[_NET_WM_WINDOW_TYPE_TOOLBAR]) ||
        xcb_reply_contains_atom(reply, atoms[_NET_WM_WINDOW_TYPE_SPLASH])) {
        LOG("This window is a dialog window, setting floating\n");
        want_floating = true;
    }

    if (cwindow->transient_for != XCB_NONE ||
        (cwindow->leader != XCB_NONE &&
         cwindow->leader != cwindow->id &&
         con_by_window_id(cwindow->leader) != NULL))
        want_floating = true;

    if (want_floating) {
        nc->rect.x = geom->x;
        nc->rect.y = geom->y;
        /* We respect the geometry wishes of floating windows, as long as they
         * are bigger than our minimal useful size (75x50). */
        nc->rect.width = max(geom->width, 75);
        nc->rect.height = max(geom->height, 50);
        LOG("geometry = %d x %d\n", nc->rect.width, nc->rect.height);
        floating_enable(nc, false);
    }

    /* to avoid getting an UnmapNotify event due to reparenting, we temporarily
     * declare no interest in any state change event of this window */
    values[0] = XCB_NONE;
    xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK, values);

    xcb_void_cookie_t rcookie = xcb_reparent_window_checked(conn, window, nc->frame, 0, 0);
    if (xcb_request_check(conn, rcookie) != NULL) {
        LOG("Could not reparent the window, aborting\n");
        goto out;
    }

    mask = XCB_CW_EVENT_MASK;
    values[0] = CHILD_EVENT_MASK;
    xcb_change_window_attributes(conn, window, mask, values);

    reply = xcb_get_property_reply(conn, state_cookie, NULL);
    if (xcb_reply_contains_atom(reply, atoms[_NET_WM_STATE_FULLSCREEN]))
        con_toggle_fullscreen(nc);

    /* Put the client inside the save set. Upon termination (whether killed or
     * normal exit does not matter) of the window manager, these clients will
     * be correctly reparented to their most closest living ancestor (=
     * cleanup) */
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, window);

    tree_render();

    free(geom);
out:
    free(attr);
    return;
}

#if 0
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
                     xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
                     int16_t x, int16_t y, uint16_t width, uint16_t height,
                     uint32_t border_width) {

       /* Minimum useful size for managed windows is 75x50 (primarily affects floating) */
        width = max(width, 75);
        height = max(height, 50);

        if (config.default_border != NULL)
                client_init_border(conn, new, config.default_border[1]);

        /* We need to grab the mouse buttons for click to focus */
        xcb_grab_button(conn, false, child, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                        1 /* left mouse button */,
                        XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

        xcb_grab_button(conn, false, child, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                        3 /* right mouse button */,
                        XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

        if (dock) {
            DLOG("Window is a dock.\n");
            Output *t_out = get_output_containing(x, y);
            if (t_out != c_ws->output) {
                    DLOG("Dock client requested to be on output %s by geometry (%d, %d)\n",
                                    t_out->name, x, y);
                    new->workspace = t_out->current_workspace;
            }
            new->dock = true;
            new->borderless = true;
            new->titlebar_position = TITLEBAR_OFF;
            new->force_reconfigure = true;
            new->container = NULL;
            SLIST_INSERT_HEAD(&(t_out->dock_clients), new, dock_clients);
            /* If it’s a dock we can’t make it float, so we break */
            new->floating = FLOATING_AUTO_OFF;
        }

        /* All clients which have a leader should be floating */
        if (!new->dock && !client_is_floating(new) && new->leader != 0) {
                DLOG("Client has WM_CLIENT_LEADER hint set, setting floating\n");
                new->floating = FLOATING_AUTO_ON;
        }

        if (new->workspace->auto_float) {
                new->floating = FLOATING_AUTO_ON;
                DLOG("workspace is in autofloat mode, setting floating\n");
        }

        if (new->dock) {
                /* Get _NET_WM_STRUT_PARTIAL to determine the client’s requested height */
                uint32_t *strut;
                preply = xcb_get_property_reply(conn, strut_cookie, NULL);
                if (preply != NULL && preply->value_len > 0 && (strut = xcb_get_property_value(preply))) {
                        /* We only use a subset of the provided values, namely the reserved space at the top/bottom
                           of the screen. This is because the only possibility for bars is at to be at the top/bottom
                           with maximum horizontal size.
                           TODO: bars at the top */
                        new->desired_height = strut[3];
                        if (new->desired_height == 0) {
                                DLOG("Client wanted to be 0 pixels high, using the window's height (%d)\n", original_height);
                                new->desired_height = original_height;
                        }
                        DLOG("the client wants to be %d pixels high\n", new->desired_height);
                } else {
                        DLOG("The client didn't specify space to reserve at the screen edge, using its height (%d)\n", original_height);
                        new->desired_height = original_height;
                }
        } else {
                /* If it’s not a dock, we can check on which workspace we should put it. */

                /* Firstly, we need to get the window’s class / title. We asked for the properties at the
                 * top of this function, get them now and pass them to our callback function for window class / title
                 * changes. It is important that the client was already inserted into the by_child table,
                 * because the callbacks won’t work otherwise. */
                preply = xcb_get_property_reply(conn, utf8_title_cookie, NULL);
                handle_windowname_change(NULL, conn, 0, new->child, atoms[_NET_WM_NAME], preply);

                preply = xcb_get_property_reply(conn, title_cookie, NULL);
                handle_windowname_change_legacy(NULL, conn, 0, new->child, WM_NAME, preply);

                preply = xcb_get_property_reply(conn, class_cookie, NULL);
                handle_windowclass_change(NULL, conn, 0, new->child, WM_CLASS, preply);

                /* if WM_CLIENT_LEADER is set, we put the new window on the
                 * same window as its leader. This might be overwritten by
                 * assignments afterwards. */
                if (new->leader != XCB_NONE) {
                        DLOG("client->leader is set (to 0x%08x)\n", new->leader);
                        Client *parent = table_get(&by_child, new->leader);
                        if (parent != NULL && parent->container != NULL) {
                                Workspace *t_ws = parent->workspace;
                                new->container = t_ws->table[parent->container->col][parent->container->row];
                                new->workspace = t_ws;
                                old_focused = new->container->currently_focused;
                                map_frame = workspace_is_visible(t_ws);
                                new->urgent = true;
                                /* This is a little tricky: we cannot use
                                 * workspace_update_urgent_flag() because the
                                 * new window was not yet inserted into the
                                 * focus stack on t_ws. */
                                t_ws->urgent = true;
                        } else {
                                DLOG("parent is not usable\n");
                        }
                }

                struct Assignment *assign;
                TAILQ_FOREACH(assign, &assignments, assignments) {
                        if (get_matching_client(conn, assign->windowclass_title, new) == NULL)
                                continue;

                        if (assign->floating == ASSIGN_FLOATING_ONLY ||
                            assign->floating == ASSIGN_FLOATING) {
                                new->floating = FLOATING_AUTO_ON;
                                LOG("Assignment matches, putting client into floating mode\n");
                                if (assign->floating == ASSIGN_FLOATING_ONLY)
                                        break;
                        }

                        LOG("Assignment \"%s\" matches, so putting it on workspace %d\n",
                            assign->windowclass_title, assign->workspace);

                        if (c_ws->output->current_workspace->num == (assign->workspace-1)) {
                                DLOG("We are already there, no need to do anything\n");
                                break;
                        }

                        DLOG("Changing container/workspace and unmapping the client\n");
                        Workspace *t_ws = workspace_get(assign->workspace-1);
                        workspace_initialize(t_ws, c_ws->output, false);

                        new->container = t_ws->table[t_ws->current_col][t_ws->current_row];
                        new->workspace = t_ws;
                        old_focused = new->container->currently_focused;

                        map_frame = workspace_is_visible(t_ws);
                        break;
                }
        }


        if (client_is_floating(new)) {
                SLIST_INSERT_HEAD(&(new->workspace->focus_stack), new, focus_clients);

                /* Add the client to the list of floating clients for its workspace */
                TAILQ_INSERT_TAIL(&(new->workspace->floating_clients), new, floating_clients);

                new->container = NULL;

                new->rect.width = new->floating_rect.width + 2 + 2;
                new->rect.height = new->floating_rect.height + (font->height + 2 + 2) + 2;

                /* Some clients (like GIMP’s color picker window) get mapped
                 * to (0, 0), so we push them to a reasonable position
                 * (centered over their leader) */
                if (new->leader != 0 && x == 0 && y == 0) {
                        DLOG("Floating client wants to (0x0), moving it over its leader instead\n");
                        Client *leader = table_get(&by_child, new->leader);
                        if (leader == NULL) {
                                DLOG("leader is NULL, centering it over current workspace\n");

                                x = c_ws->rect.x + (c_ws->rect.width / 2) - (new->rect.width / 2);
                                y = c_ws->rect.y + (c_ws->rect.height / 2) - (new->rect.height / 2);
                        } else {
                                x = leader->rect.x + (leader->rect.width / 2) - (new->rect.width / 2);
                                y = leader->rect.y + (leader->rect.height / 2) - (new->rect.height / 2);
                        }
                }
                new->floating_rect.x = new->rect.x = x;
                new->floating_rect.y = new->rect.y = y;
                DLOG("copying floating_rect from tiling (%d, %d) size (%d, %d)\n",
                                new->floating_rect.x, new->floating_rect.y,
                                new->floating_rect.width, new->floating_rect.height);
                DLOG("outer rect (%d, %d) size (%d, %d)\n",
                                new->rect.x, new->rect.y, new->rect.width, new->rect.height);

                /* Make sure it is on top of the other windows */
                xcb_raise_window(conn, new->frame);
                reposition_client(conn, new);
                resize_client(conn, new);
                /* redecorate_window flushes */
                redecorate_window(conn, new);
        }

        new->initialized = true;


        render_layout(conn);

map:
        /* Map the window first to avoid flickering */
        xcb_map_window(conn, child);
        if (map_frame)
                client_map(conn, new);

        if ((CUR_CELL->workspace->fullscreen_client == NULL || new->fullscreen) && !new->dock) {
                /* Focus the new window if we’re not in fullscreen mode and if it is not a dock window */
                if ((new->workspace->fullscreen_client == NULL) || new->fullscreen) {
                        if (!client_is_floating(new)) {
                                new->container->currently_focused = new;
                                if (map_frame)
                                        render_container(conn, new->container);
                        }
                        if (new->container == CUR_CELL || client_is_floating(new)) {
                                xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, new->child, XCB_CURRENT_TIME);
                                ewmh_update_active_window(new->child);
                        }
                }
        }

        xcb_flush(conn);
}
#endif
