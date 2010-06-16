/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * src/manage.c: Contains all functions for initially managing new windows
 *               (or existing ones on restart).
 *
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "xcb.h"
#include "data.h"
#include "util.h"
#include "i3.h"
#include "table.h"
#include "config.h"
#include "handlers.h"
#include "layout.h"
#include "manage.h"
#include "floating.h"
#include "client.h"
#include "workspace.h"
#include "log.h"
#include "ewmh.h"

/*
 * Go through all existing windows (if the window manager is restarted) and manage them
 *
 */
void manage_existing_windows(xcb_connection_t *conn, xcb_property_handlers_t *prophs, xcb_window_t root) {
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
                manage_window(prophs, conn, children[i], cookies[i], true);

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
void restore_geometry(xcb_connection_t *conn) {
        Workspace *ws;
        Client *client;
        DLOG("Restoring geometry\n");

        TAILQ_FOREACH(ws, workspaces, workspaces)
                SLIST_FOREACH(client, &(ws->focus_stack), focus_clients)
                        xcb_reparent_window(conn, client->child, root,
                                            client->rect.x, client->rect.y);

        /* Make sure our changes reach the X server, we restart/exit now */
        xcb_flush(conn);
}

/*
 * Do some sanity checks and then reparent the window.
 *
 */
void manage_window(xcb_property_handlers_t *prophs, xcb_connection_t *conn,
                   xcb_window_t window, xcb_get_window_attributes_cookie_t cookie,
                   bool needs_to_be_mapped) {
        xcb_drawable_t d = { window };
        xcb_get_geometry_cookie_t geomc;
        xcb_get_geometry_reply_t *geom;
        xcb_get_window_attributes_reply_t *attr = 0;

        geomc = xcb_get_geometry(conn, d);

        /* Check if the window is mapped (it could be not mapped when intializing and
           calling manage_window() for every window) */
        if ((attr = xcb_get_window_attributes_reply(conn, cookie, 0)) == NULL) {
                ELOG("Could not get attributes\n");
                return;
        }

        if (needs_to_be_mapped && attr->map_state != XCB_MAP_STATE_VIEWABLE)
                goto out;

        /* Don’t manage clients with the override_redirect flag */
        if (attr->override_redirect)
                goto out;

        /* Check if the window is already managed */
        if (table_get(&by_child, window))
                goto out;

        /* Get the initial geometry (position, size, …) */
        if ((geom = xcb_get_geometry_reply(conn, geomc, 0)) == NULL)
                goto out;

        /* Reparent the window and add it to our list of managed windows */
        reparent_window(conn, window, attr->visual, geom->root, geom->depth,
                        geom->x, geom->y, geom->width, geom->height,
                        geom->border_width);

        /* Generate callback events for every property we watch */
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_CLASS);
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_NAME);
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_NORMAL_HINTS);
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_HINTS);
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_TRANSIENT_FOR);
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, atoms[WM_CLIENT_LEADER]);
        xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, atoms[_NET_WM_NAME]);

        free(geom);
out:
        free(attr);
        return;
}

/*
 * reparent_window() gets called when a new window was opened and becomes a child of the root
 * window, or it gets called by us when we manage the already existing windows at startup.
 *
 * Essentially, this is the point where we take over control.
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
                     xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
                     int16_t x, int16_t y, uint16_t width, uint16_t height,
                     uint32_t border_width) {

        xcb_get_property_cookie_t wm_type_cookie, strut_cookie, state_cookie,
                                  utf8_title_cookie, title_cookie,
                                  class_cookie, leader_cookie;
        uint32_t mask = 0;
        uint32_t values[3];
        uint16_t original_height = height;
        bool map_frame = true;

        /* We are interested in property changes */
        mask = XCB_CW_EVENT_MASK;
        values[0] = CHILD_EVENT_MASK;
        xcb_change_window_attributes(conn, child, mask, values);

        /* Place requests for properties ASAP */
        wm_type_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[_NET_WM_WINDOW_TYPE], UINT32_MAX);
        strut_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[_NET_WM_STRUT_PARTIAL], UINT32_MAX);
        state_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[_NET_WM_STATE], UINT32_MAX);
        utf8_title_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[_NET_WM_NAME], 128);
        leader_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[WM_CLIENT_LEADER], UINT32_MAX);
        title_cookie = xcb_get_any_property_unchecked(conn, false, child, WM_NAME, 128);
        class_cookie = xcb_get_any_property_unchecked(conn, false, child, WM_CLASS, 128);

        Client *new = table_get(&by_child, child);

        /* Events for already managed windows should already be filtered in manage_window() */
        assert(new == NULL);

        LOG("Managing window 0x%08x\n", child);
        DLOG("x = %d, y = %d, width = %d, height = %d\n", x, y, width, height);
        new = scalloc(sizeof(Client));
        new->force_reconfigure = true;

        /* Update the data structures */
        Client *old_focused = CUR_CELL->currently_focused;

        new->container = CUR_CELL;
        new->workspace = new->container->workspace;

        /* Minimum useful size for managed windows is 75x50 (primarily affects floating) */
        width = max(width, 75);
        height = max(height, 50);

        new->frame = xcb_generate_id(conn);
        new->child = child;
        new->rect.width = width;
        new->rect.height = height;
        new->width_increment = 1;
        new->height_increment = 1;
        new->border_width = border_width;
        /* Pre-initialize the values for floating */
        new->floating_rect.x = -1;
        new->floating_rect.width = width;
        new->floating_rect.height = height;

        if (config.default_border != NULL)
                client_init_border(conn, new, config.default_border[1]);

        mask = 0;

        /* Don’t generate events for our new window, it should *not* be managed */
        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[0] = 1;

        /* We want to know when… */
        mask |= XCB_CW_EVENT_MASK;
        values[1] = FRAME_EVENT_MASK;

        i3Font *font = load_font(conn, config.font);
        width = min(width, c_ws->rect.x + c_ws->rect.width);
        height = min(height, c_ws->rect.y + c_ws->rect.height);

        Rect framerect = {x, y,
                          width + 2 + 2,                  /* 2 px border at each side */
                          height + 2 + 2 + font->height}; /* 2 px border plus font’s height */

        /* Yo dawg, I heard you like windows, so I create a window around your window… */
        new->frame = create_window(conn, framerect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, false, mask, values);

        /* Put the client inside the save set. Upon termination (whether killed or normal exit
           does not matter) of the window manager, these clients will be correctly reparented
           to their most closest living ancestor (= cleanup) */
        xcb_change_save_set(conn, XCB_SET_MODE_INSERT, child);

        /* Generate a graphics context for the titlebar */
        new->titlegc = xcb_generate_id(conn);
        xcb_create_gc(conn, new->titlegc, new->frame, 0, 0);

        /* Moves the original window into the new frame we've created for it */
        new->awaiting_useless_unmap = true;
        xcb_void_cookie_t cookie = xcb_reparent_window_checked(conn, child, new->frame, 0, font->height);
        if (xcb_request_check(conn, cookie) != NULL) {
                DLOG("Could not reparent the window, aborting\n");
                xcb_destroy_window(conn, new->frame);
                free(new);
                return;
        }

        /* Put our data structure (Client) into the table */
        table_put(&by_parent, new->frame, new);
        table_put(&by_child, child, new);

        /* We need to grab the mouse buttons for click to focus */
        xcb_grab_button(conn, false, child, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                        1 /* left mouse button */,
                        XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

        xcb_grab_button(conn, false, child, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                        3 /* right mouse button */,
                        XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

        /* Get _NET_WM_WINDOW_TYPE (to see if it’s a dock) */
        xcb_atom_t *atom;
        xcb_get_property_reply_t *preply = xcb_get_property_reply(conn, wm_type_cookie, NULL);
        if (preply != NULL && preply->value_len > 0 && (atom = xcb_get_property_value(preply))) {
                for (int i = 0; i < xcb_get_property_value_length(preply); i++)
                        if (atom[i] == atoms[_NET_WM_WINDOW_TYPE_DOCK]) {
                                DLOG("Window is a dock.\n");
                                Output *t_out = get_output_containing(x, y);
                                if (t_out == NULL)
                                        t_out = c_ws->output;
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
                                break;
                        } else if (atom[i] == atoms[_NET_WM_WINDOW_TYPE_DIALOG] ||
                                   atom[i] == atoms[_NET_WM_WINDOW_TYPE_UTILITY] ||
                                   atom[i] == atoms[_NET_WM_WINDOW_TYPE_TOOLBAR] ||
                                   atom[i] == atoms[_NET_WM_WINDOW_TYPE_SPLASH]) {
                                /* Set the dialog window to automatically floating, will be used below */
                                new->floating = FLOATING_AUTO_ON;
                                DLOG("dialog/utility/toolbar/splash window, automatically floating\n");
                        }
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

                preply = xcb_get_property_reply(conn, leader_cookie, NULL);
                handle_clientleader_change(NULL, conn, 0, new->child, atoms[WM_CLIENT_LEADER], preply);

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

        if (new->workspace->fullscreen_client != NULL) {
                DLOG("Setting below fullscreen window\n");

                /* If we are in fullscreen, we should place the window below
                 * the fullscreen window to not be annoying */
                uint32_t values[] = {
                        new->workspace->fullscreen_client->frame,
                        XCB_STACK_MODE_BELOW
                };
                xcb_configure_window(conn, new->frame,
                                     XCB_CONFIG_WINDOW_SIBLING |
                                     XCB_CONFIG_WINDOW_STACK_MODE, values);
        }

        /* Insert into the currently active container, if it’s not a dock window */
        if (!new->dock && !client_is_floating(new)) {
                /* Insert after the old active client, if existing. If it does not exist, the
                   container is empty and it does not matter, where we insert it */
                if (old_focused != NULL && !old_focused->dock)
                        CIRCLEQ_INSERT_AFTER(&(new->container->clients), old_focused, new, clients);
                else CIRCLEQ_INSERT_TAIL(&(new->container->clients), new, clients);

                if (new->container->workspace->fullscreen_client != NULL)
                        SLIST_INSERT_AFTER(new->container->workspace->fullscreen_client, new, focus_clients);
                else SLIST_INSERT_HEAD(&(new->container->workspace->focus_stack), new, focus_clients);

                client_set_below_floating(conn, new);
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

        /* Check if the window already got the fullscreen hint set */
        xcb_atom_t *state;
        if ((preply = xcb_get_property_reply(conn, state_cookie, NULL)) != NULL &&
            (state = xcb_get_property_value(preply)) != NULL)
                /* Check all set _NET_WM_STATEs */
                for (int i = 0; i < xcb_get_property_value_length(preply); i++) {
                        if (state[i] != atoms[_NET_WM_STATE_FULLSCREEN])
                                continue;
                        /* If the window got the fullscreen state, we just toggle fullscreen
                           and don’t event bother to redraw the layout – that would not change
                           anything anyways */
                        client_toggle_fullscreen(conn, new);
                        goto map;
                }

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
