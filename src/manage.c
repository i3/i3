/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * manage.c: Initially managing new windows (or existing ones on restart).
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
    DLOG("Restoring geometry\n");

    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons)
        if (con->window) {
            DLOG("Re-adding X11 border of %d px\n", con->border_width);
            con->window_rect.width += (2 * con->border_width);
            con->window_rect.height += (2 * con->border_width);
            xcb_set_window_rect(conn, con->window->id, con->window_rect);
            DLOG("placing window %08x at %d %d\n", con->window->id, con->rect.x, con->rect.y);
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
    xcb_get_window_attributes_reply_t *attr = NULL;

    xcb_get_property_cookie_t wm_type_cookie, strut_cookie, state_cookie,
                              utf8_title_cookie, title_cookie,
                              class_cookie, leader_cookie, transient_cookie,
                              role_cookie, startup_id_cookie;


    geomc = xcb_get_geometry(conn, d);
#define FREE_GEOMETRY() do { \
    if ((geom = xcb_get_geometry_reply(conn, geomc, 0)) != NULL) \
        free(geom); \
} while (0)

    /* Check if the window is mapped (it could be not mapped when intializing and
       calling manage_window() for every window) */
    if ((attr = xcb_get_window_attributes_reply(conn, cookie, 0)) == NULL) {
        DLOG("Could not get attributes\n");
        FREE_GEOMETRY();
        return;
    }

    if (needs_to_be_mapped && attr->map_state != XCB_MAP_STATE_VIEWABLE) {
        FREE_GEOMETRY();
        goto out;
    }

    /* Don’t manage clients with the override_redirect flag */
    if (attr->override_redirect) {
        FREE_GEOMETRY();
        goto out;
    }

    /* Check if the window is already managed */
    if (con_by_window_id(window) != NULL) {
        DLOG("already managed (by con %p)\n", con_by_window_id(window));
        FREE_GEOMETRY();
        goto out;
    }

    /* Get the initial geometry (position, size, …) */
    if ((geom = xcb_get_geometry_reply(conn, geomc, 0)) == NULL) {
        DLOG("could not get geometry\n");
        goto out;
    }

    uint32_t values[1];

    /* Set a temporary event mask for the new window, consisting only of
     * PropertyChange. We need to be notified of PropertyChanges because the
     * client can change its properties *after* we requested them but *before*
     * we actually reparented it and have set our final event mask. */
    values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK, values);

#define GET_PROPERTY(atom, len) xcb_get_property(conn, false, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, len)

    wm_type_cookie = GET_PROPERTY(A__NET_WM_WINDOW_TYPE, UINT32_MAX);
    strut_cookie = GET_PROPERTY(A__NET_WM_STRUT_PARTIAL, UINT32_MAX);
    state_cookie = GET_PROPERTY(A__NET_WM_STATE, UINT32_MAX);
    utf8_title_cookie = GET_PROPERTY(A__NET_WM_NAME, 128);
    leader_cookie = GET_PROPERTY(A_WM_CLIENT_LEADER, UINT32_MAX);
    transient_cookie = GET_PROPERTY(XCB_ATOM_WM_TRANSIENT_FOR, UINT32_MAX);
    title_cookie = GET_PROPERTY(XCB_ATOM_WM_NAME, 128);
    class_cookie = GET_PROPERTY(XCB_ATOM_WM_CLASS, 128);
    role_cookie = GET_PROPERTY(A_WM_WINDOW_ROLE, 128);
    startup_id_cookie = GET_PROPERTY(A__NET_STARTUP_ID, 512);
    /* TODO: also get wm_normal_hints here. implement after we got rid of xcb-event */

    DLOG("Managing window 0x%08x\n", window);

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
    window_update_class(cwindow, xcb_get_property_reply(conn, class_cookie, NULL), true);
    window_update_name_legacy(cwindow, xcb_get_property_reply(conn, title_cookie, NULL), true);
    window_update_name(cwindow, xcb_get_property_reply(conn, utf8_title_cookie, NULL), true);
    window_update_leader(cwindow, xcb_get_property_reply(conn, leader_cookie, NULL));
    window_update_transient_for(cwindow, xcb_get_property_reply(conn, transient_cookie, NULL));
    window_update_strut_partial(cwindow, xcb_get_property_reply(conn, strut_cookie, NULL));
    window_update_role(cwindow, xcb_get_property_reply(conn, role_cookie, NULL), true);

    xcb_get_property_reply_t *startup_id_reply;
    startup_id_reply = xcb_get_property_reply(conn, startup_id_cookie, NULL);
    char *startup_ws = startup_workspace_for_window(cwindow, startup_id_reply);
    DLOG("startup workspace = %s\n", startup_ws);

    /* check if the window needs WM_TAKE_FOCUS */
    cwindow->needs_take_focus = window_supports_protocol(cwindow->id, A_WM_TAKE_FOCUS);

    /* Where to start searching for a container that swallows the new one? */
    Con *search_at = croot;

    xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, wm_type_cookie, NULL);
    if (xcb_reply_contains_atom(reply, A__NET_WM_WINDOW_TYPE_DOCK)) {
        LOG("This window is of type dock\n");
        Output *output = get_output_containing(geom->x, geom->y);
        if (output != NULL) {
            DLOG("Starting search at output %s\n", output->name);
            search_at = output->con;
        }

        /* find out the desired position of this dock window */
        if (cwindow->reserved.top > 0 && cwindow->reserved.bottom == 0) {
            DLOG("Top dock client\n");
            cwindow->dock = W_DOCK_TOP;
        } else if (cwindow->reserved.top == 0 && cwindow->reserved.bottom > 0) {
            DLOG("Bottom dock client\n");
            cwindow->dock = W_DOCK_BOTTOM;
        } else {
            DLOG("Ignoring invalid reserved edges (_NET_WM_STRUT_PARTIAL), using position as fallback:\n");
            if (geom->y < (search_at->rect.height / 2)) {
                DLOG("geom->y = %d < rect.height / 2 = %d, it is a top dock client\n",
                     geom->y, (search_at->rect.height / 2));
                cwindow->dock = W_DOCK_TOP;
            } else {
                DLOG("geom->y = %d >= rect.height / 2 = %d, it is a bottom dock client\n",
                     geom->y, (search_at->rect.height / 2));
                cwindow->dock = W_DOCK_BOTTOM;
            }
        }
    }

    DLOG("Initial geometry: (%d, %d, %d, %d)\n", geom->x, geom->y, geom->width, geom->height);

    Con *nc = NULL;
    Match *match;
    Assignment *assignment;

    /* TODO: two matches for one container */

    /* See if any container swallows this new window */
    nc = con_for_window(search_at, cwindow, &match);
    if (nc == NULL) {
        /* If not, check if it is assigned to a specific workspace / output */
        if ((assignment = assignment_for(cwindow, A_TO_WORKSPACE | A_TO_OUTPUT))) {
            DLOG("Assignment matches (%p)\n", match);
            if (assignment->type == A_TO_WORKSPACE) {
                nc = con_descend_tiling_focused(workspace_get(assignment->dest.workspace, NULL));
                DLOG("focused on ws %s: %p / %s\n", assignment->dest.workspace, nc, nc->name);
                if (nc->type == CT_WORKSPACE)
                    nc = tree_open_con(nc, cwindow);
                else nc = tree_open_con(nc->parent, cwindow);
            }
        /* TODO: handle assignments with type == A_TO_OUTPUT */
        } else if (startup_ws) {
            /* If it’s not assigned, but was started on a specific workspace,
             * we want to open it there */
            DLOG("Using workspace on which this application was started (%s)\n", startup_ws);
            nc = con_descend_tiling_focused(workspace_get(startup_ws, NULL));
            DLOG("focused on ws %s: %p / %s\n", startup_ws, nc, nc->name);
            if (nc->type == CT_WORKSPACE)
                nc = tree_open_con(nc, cwindow);
            else nc = tree_open_con(nc->parent, cwindow);
        } else {
            /* If not, insert it at the currently focused position */
            if (focused->type == CT_CON && con_accepts_window(focused)) {
                LOG("using current container, focused = %p, focused->name = %s\n",
                                focused, focused->name);
                nc = focused;
            } else nc = tree_open_con(NULL, cwindow);
        }
    } else {
        /* M_BELOW inserts the new window as a child of the one which was
         * matched (e.g. dock areas) */
        if (match != NULL && match->insert_where == M_BELOW) {
            nc = tree_open_con(nc, cwindow);
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

    Con *ws = con_get_workspace(nc);
    Con *fs = (ws ? con_get_fullscreen_con(ws, CF_OUTPUT) : NULL);
    if (fs == NULL)
        fs = con_get_fullscreen_con(croot, CF_GLOBAL);

    if (fs == NULL) {
        DLOG("Not in fullscreen mode, focusing\n");
        if (!cwindow->dock) {
            /* Check that the workspace is visible and on the same output as
             * the current focused container. If the window was assigned to an
             * invisible workspace, we should not steal focus. */
            Con *current_output = con_get_output(focused);
            Con *target_output = con_get_output(ws);

            if (workspace_is_visible(ws) && current_output == target_output) {
                con_focus(nc);
            } else DLOG("workspace not visible, not focusing\n");
        } else DLOG("dock, not focusing\n");
    } else {
        DLOG("fs = %p, ws = %p, not focusing\n", fs, ws);
        /* Insert the new container in focus stack *after* the currently
         * focused (fullscreen) con. This way, the new container will be
         * focused after we return from fullscreen mode */
        Con *first = TAILQ_FIRST(&(nc->parent->focus_head));
        if (first != nc) {
            /* We only modify the focus stack if the container is not already
             * the first one. This can happen when existing containers swallow
             * new windows, for example when restarting. */
            TAILQ_REMOVE(&(nc->parent->focus_head), nc, focused);
            TAILQ_INSERT_AFTER(&(nc->parent->focus_head), first, nc, focused);
        }
    }

    /* set floating if necessary */
    bool want_floating = false;
    if (xcb_reply_contains_atom(reply, A__NET_WM_WINDOW_TYPE_DIALOG) ||
        xcb_reply_contains_atom(reply, A__NET_WM_WINDOW_TYPE_UTILITY) ||
        xcb_reply_contains_atom(reply, A__NET_WM_WINDOW_TYPE_TOOLBAR) ||
        xcb_reply_contains_atom(reply, A__NET_WM_WINDOW_TYPE_SPLASH)) {
        LOG("This window is a dialog window, setting floating\n");
        want_floating = true;
    }

    FREE(reply);

    if (cwindow->transient_for != XCB_NONE ||
        (cwindow->leader != XCB_NONE &&
         cwindow->leader != cwindow->id &&
         con_by_window_id(cwindow->leader) != NULL)) {
        LOG("This window is transiert for another window, setting floating\n");
        want_floating = true;

        if (config.popup_during_fullscreen == PDF_LEAVE_FULLSCREEN &&
            fs != NULL) {
            LOG("There is a fullscreen window, leaving fullscreen mode\n");
            con_toggle_fullscreen(fs, CF_OUTPUT);
        }
    }

    /* dock clients cannot be floating, that makes no sense */
    if (cwindow->dock)
        want_floating = false;

    /* Store the requested geometry. The width/height gets raised to at least
     * 75x50 when entering floating mode, which is the minimum size for a
     * window to be useful (smaller windows are usually overlays/toolbars/…
     * which are not managed by the wm anyways). We store the original geometry
     * here because it’s used for dock clients. */
    nc->geometry = (Rect){ geom->x, geom->y, geom->width, geom->height };

    if (want_floating) {
        DLOG("geometry = %d x %d\n", nc->geometry.width, nc->geometry.height);
        floating_enable(nc, true);
    }

    /* to avoid getting an UnmapNotify event due to reparenting, we temporarily
     * declare no interest in any state change event of this window */
    values[0] = XCB_NONE;
    xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK, values);

    xcb_void_cookie_t rcookie = xcb_reparent_window_checked(conn, window, nc->frame, 0, 0);
    if (xcb_request_check(conn, rcookie) != NULL) {
        LOG("Could not reparent the window, aborting\n");
        goto geom_out;
    }

    values[0] = CHILD_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;
    xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK, values);
    xcb_flush(conn);

    reply = xcb_get_property_reply(conn, state_cookie, NULL);
    if (xcb_reply_contains_atom(reply, A__NET_WM_STATE_FULLSCREEN))
        con_toggle_fullscreen(nc, CF_OUTPUT);

    FREE(reply);

    /* Put the client inside the save set. Upon termination (whether killed or
     * normal exit does not matter) of the window manager, these clients will
     * be correctly reparented to their most closest living ancestor (=
     * cleanup) */
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, window);

    /* Check if any assignments match */
    run_assignments(cwindow);

    tree_render();

geom_out:
    free(geom);
out:
    free(attr);
    return;
}
