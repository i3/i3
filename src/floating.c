/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * src/floating.c: contains all functions for handling floating clients
 *
 */


#include "all.h"

extern xcb_connection_t *conn;

void floating_enable(Con *con, bool automatic) {
    bool set_focus = true;

    if (con_is_floating(con)) {
        LOG("Container is already in floating mode, not doing anything.\n");
        return;
    }

    /* 1: If the container is a workspace container, we need to create a new
     * split-container with the same orientation and make that one floating. We
     * cannot touch the workspace container itself because floating containers
     * are children of the workspace. */
    if (con->type == CT_WORKSPACE) {
        LOG("This is a workspace, creating new container around content\n");
        if (con_num_children(con) == 0) {
            LOG("Workspace is empty, aborting\n");
            return;
        }
        /* TODO: refactor this with src/con.c:con_set_layout */
        Con *new = con_new(NULL, NULL);
        new->parent = con;
        new->orientation = con->orientation;

        /* since the new container will be set into floating mode directly
         * afterwards, we need to copy the workspace rect. */
        memcpy(&(new->rect), &(con->rect), sizeof(Rect));

        Con *old_focused = TAILQ_FIRST(&(con->focus_head));
        if (old_focused == TAILQ_END(&(con->focus_head)))
            old_focused = NULL;

        /* 4: move the existing cons of this workspace below the new con */
        DLOG("Moving cons\n");
        Con *child;
        while (!TAILQ_EMPTY(&(con->nodes_head))) {
            child = TAILQ_FIRST(&(con->nodes_head));
            con_detach(child);
            con_attach(child, new, true);
        }

        /* 4: attach the new split container to the workspace */
        DLOG("Attaching new split to ws\n");
        con_attach(new, con, false);

        if (old_focused)
            con_focus(old_focused);

        con = new;
        set_focus = false;
    }

    /* 1: detach the container from its parent */
    /* TODO: refactor this with tree_close() */
    TAILQ_REMOVE(&(con->parent->nodes_head), con, nodes);
    TAILQ_REMOVE(&(con->parent->focus_head), con, focused);

    con_fix_percent(con->parent);

    /* 2: create a new container to render the decoration on, add
     * it as a floating window to the workspace */
    Con *nc = con_new(NULL, NULL);
    /* we need to set the parent afterwards instead of passing it as an
     * argument to con_new() because nc would be inserted into the tiling layer
     * otherwise. */
    nc->parent = con_get_workspace(con);

    /* check if the parent container is empty and close it if so */
    if ((con->parent->type == CT_CON || con->parent->type == CT_FLOATING_CON) && con_num_children(con->parent) == 0) {
        DLOG("Old container empty after setting this child to floating, closing\n");
        tree_close(con->parent, DONT_KILL_WINDOW, false);
    }

    char *name;
    asprintf(&name, "[i3 con] floatingcon around %p", con);
    x_set_name(nc, name);
    free(name);

    /* find the height for the decorations */
    int deco_height = config.font.height + 5;

    DLOG("Original rect: (%d, %d) with %d x %d\n", con->rect.x, con->rect.y, con->rect.width, con->rect.height);
    Rect zero = { 0, 0, 0, 0 };
    nc->rect = con->geometry;
    /* If the geometry was not set (split containers), we need to determine a
     * sensible one by combining the geometry of all children */
    if (memcmp(&(nc->rect), &zero, sizeof(Rect)) == 0) {
        DLOG("Geometry not set, combining children\n");
        Con *child;
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            DLOG("child geometry: %d x %d\n", child->geometry.width, child->geometry.height);
            nc->rect.width += child->geometry.width;
            nc->rect.height = max(nc->rect.height, child->geometry.height);
        }
    }
    /* Raise the width/height to at least 75x50 (minimum size for windows) */
    nc->rect.width = max(nc->rect.width, 75);
    nc->rect.height = max(nc->rect.height, 50);
    /* add pixels for the decoration */
    /* TODO: don’t add them when the user automatically puts new windows into
     * 1pixel/borderless mode */
    nc->rect.height += deco_height + 4;
    nc->rect.width += 4;
    DLOG("Floating rect: (%d, %d) with %d x %d\n", nc->rect.x, nc->rect.y, nc->rect.width, nc->rect.height);
    nc->orientation = NO_ORIENTATION;
    nc->type = CT_FLOATING_CON;
    TAILQ_INSERT_TAIL(&(nc->parent->floating_head), nc, floating_windows);
    TAILQ_INSERT_TAIL(&(nc->parent->focus_head), nc, focused);

    /* 3: attach the child to the new parent container */
    con->parent = nc;
    con->percent = 1.0;
    con->floating = FLOATING_USER_ON;

    /* Some clients (like GIMP’s color picker window) get mapped
     * to (0, 0), so we push them to a reasonable position
     * (centered over their leader) */
    if (nc->rect.x == 0 && nc->rect.y == 0) {
        Con *leader;
        if (con->window && con->window->leader != XCB_NONE &&
            (leader = con_by_window_id(con->window->leader)) != NULL) {
            DLOG("Centering above leader\n");
            nc->rect.x = leader->rect.x + (leader->rect.width / 2) - (nc->rect.width / 2);
            nc->rect.y = leader->rect.y + (leader->rect.height / 2) - (nc->rect.height / 2);
        } else {
            /* center the window on workspace as fallback */
            Con *ws = nc->parent;
            nc->rect.x = ws->rect.x + (ws->rect.width / 2) - (nc->rect.width / 2);
            nc->rect.y = ws->rect.y + (ws->rect.height / 2) - (nc->rect.height / 2);
        }
    }

    TAILQ_INSERT_TAIL(&(nc->nodes_head), con, nodes);
    TAILQ_INSERT_TAIL(&(nc->focus_head), con, focused);

    /* render the cons to get initial window_rect correct */
    render_con(nc, false);
    render_con(con, false);

    // TODO: don’t influence focus handling when Con was not focused before.
    if (set_focus)
        con_focus(con);

    /* Check if we need to re-assign it to a different workspace because of its
     * coordinates and exit if that was done successfully. */
    if (floating_maybe_reassign_ws(nc))
        return;

    /* Sanitize coordinates: Check if they are on any output */
    if (get_output_containing(nc->rect.x, nc->rect.y) != NULL)
        return;

    ELOG("No output found at destination coordinates, centering floating window on current ws\n");
    Con *ws = nc->parent;
    nc->rect.x = ws->rect.x + (ws->rect.width / 2) - (nc->rect.width / 2);
    nc->rect.y = ws->rect.y + (ws->rect.height / 2) - (nc->rect.height / 2);
}

void floating_disable(Con *con, bool automatic) {
    if (!con_is_floating(con)) {
        LOG("Container isn't floating, not doing anything.\n");
        return;
    }

    Con *ws = con_get_workspace(con);

    /* 1: detach from parent container */
    TAILQ_REMOVE(&(con->parent->nodes_head), con, nodes);
    TAILQ_REMOVE(&(con->parent->focus_head), con, focused);

    /* 2: kill parent container */
    TAILQ_REMOVE(&(con->parent->parent->floating_head), con->parent, floating_windows);
    TAILQ_REMOVE(&(con->parent->parent->focus_head), con->parent, focused);
    tree_close(con->parent, DONT_KILL_WINDOW, false);

    /* 3: re-attach to the parent of the currently focused con on the workspace
     * this floating con was on */
    Con *focused = con_descend_tiling_focused(ws);

    /* if there is no other container on this workspace, focused will be the
     * workspace itself */
    if (focused->type == CT_WORKSPACE)
        con->parent = focused;
    else con->parent = focused->parent;

    /* con_fix_percent will adjust the percent value */
    con->percent = 0.0;

    TAILQ_INSERT_TAIL(&(con->parent->nodes_head), con, nodes);
    TAILQ_INSERT_TAIL(&(con->parent->focus_head), con, focused);

    con->floating = FLOATING_USER_OFF;

    con_fix_percent(con->parent);
    // TODO: don’t influence focus handling when Con was not focused before.
    con_focus(con);
}

/*
 * Toggles floating mode for the given container.
 *
 * If the automatic flag is set to true, this was an automatic update by a change of the
 * window class from the application which can be overwritten by the user.
 *
 */
void toggle_floating_mode(Con *con, bool automatic) {
    /* see if the client is already floating */
    if (con_is_floating(con)) {
        LOG("already floating, re-setting to tiling\n");

        floating_disable(con, automatic);
        return;
    }

    floating_enable(con, automatic);
}

/*
 * Raises the given container in the list of floating containers
 *
 */
void floating_raise_con(Con *con) {
    DLOG("Raising floating con %p / %s\n", con, con->name);
    TAILQ_REMOVE(&(con->parent->floating_head), con, floating_windows);
    TAILQ_INSERT_TAIL(&(con->parent->floating_head), con, floating_windows);
}

/*
 * Checks if con’s coordinates are within its workspace and re-assigns it to
 * the actual workspace if not.
 *
 */
bool floating_maybe_reassign_ws(Con *con) {
    Output *output = get_output_containing(
        con->rect.x + (con->rect.width / 2),
        con->rect.y + (con->rect.height / 2));

    if (!output) {
        ELOG("No output found at destination coordinates?\n");
        return false;
    }

    if (con_get_output(con) == output->con) {
        DLOG("still the same ws\n");
        return false;
    }

    DLOG("Need to re-assign!\n");

    Con *content = output_get_content(output->con);
    Con *ws = TAILQ_FIRST(&(content->focus_head));
    DLOG("Moving con %p / %s to workspace %p / %s\n", con, con->name, ws, ws->name);
    con_move_to_workspace(con, ws);
    con_focus(con_descend_focused(con));
    return true;
}

DRAGGING_CB(drag_window_callback) {
    struct xcb_button_press_event_t *event = extra;

    /* Reposition the client correctly while moving */
    con->rect.x = old_rect->x + (new_x - event->root_x);
    con->rect.y = old_rect->y + (new_y - event->root_y);

    render_con(con, false);
    x_push_node(con);
    xcb_flush(conn);

    /* Check if we cross workspace boundaries while moving */
    if (!floating_maybe_reassign_ws(con))
        return;
    tree_render();
}

/*
 * Called when the user clicked on the titlebar of a floating window.
 * Calls the drag_pointer function with the drag_window callback
 *
 */
void floating_drag_window(Con *con, xcb_button_press_event_t *event) {
    DLOG("floating_drag_window\n");

    drag_pointer(con, event, XCB_NONE, BORDER_TOP /* irrelevant */, drag_window_callback, event);
    tree_render();
}

/*
 * This is an ugly data structure which we need because there is no standard
 * way of having nested functions (only available as a gcc extension at the
 * moment, clang doesn’t support it) or blocks (only available as a clang
 * extension and only on Mac OS X systems at the moment).
 *
 */
struct resize_window_callback_params {
    border_t corner;
    bool proportional;
    xcb_button_press_event_t *event;
};

DRAGGING_CB(resize_window_callback) {
    struct resize_window_callback_params *params = extra;
    xcb_button_press_event_t *event = params->event;
    border_t corner = params->corner;

    int32_t dest_x = con->rect.x;
    int32_t dest_y = con->rect.y;
    uint32_t dest_width;
    uint32_t dest_height;

    double ratio = (double) old_rect->width / old_rect->height;

    /* First guess: We resize by exactly the amount the mouse moved,
     * taking into account in which corner the client was grabbed */
    if (corner & BORDER_LEFT)
        dest_width = old_rect->width - (new_x - event->root_x);
    else dest_width = old_rect->width + (new_x - event->root_x);

    if (corner & BORDER_TOP)
        dest_height = old_rect->height - (new_y - event->root_y);
    else dest_height = old_rect->height + (new_y - event->root_y);

    /* Obey minimum window size */
    Rect minimum = con_minimum_size(con);
    dest_width = max(dest_width, minimum.width);
    dest_height = max(dest_height, minimum.height);

    /* User wants to keep proportions, so we may have to adjust our values */
    if (params->proportional) {
        dest_width = max(dest_width, (int) (dest_height * ratio));
        dest_height = max(dest_height, (int) (dest_width / ratio));
    }

    /* If not the lower right corner is grabbed, we must also reposition
     * the client by exactly the amount we resized it */
    if (corner & BORDER_LEFT)
        dest_x = old_rect->x + (old_rect->width - dest_width);

    if (corner & BORDER_TOP)
        dest_y = old_rect->y + (old_rect->height - dest_height);

    con->rect = (Rect) { dest_x, dest_y, dest_width, dest_height };

    /* TODO: don’t re-render the whole tree just because we change
     * coordinates of a floating window */
    tree_render();
    x_push_changes(croot);
}

/*
 * Called when the user clicked on a floating window while holding the
 * floating_modifier and the right mouse button.
 * Calls the drag_pointer function with the resize_window callback
 *
 */
void floating_resize_window(Con *con, bool proportional,
                            xcb_button_press_event_t *event) {
    DLOG("floating_resize_window\n");

    /* corner saves the nearest corner to the original click. It contains
     * a bitmask of the nearest borders (BORDER_LEFT, BORDER_RIGHT, …) */
    border_t corner = 0;

    if (event->event_x <= (con->rect.width / 2))
        corner |= BORDER_LEFT;
    else corner |= BORDER_RIGHT;

    if (event->event_y <= (con->rect.height / 2))
        corner |= BORDER_TOP;
    else corner |= BORDER_BOTTOM;

    struct resize_window_callback_params params = { corner, proportional, event };

    drag_pointer(con, event, XCB_NONE, BORDER_TOP /* irrelevant */, resize_window_callback, &params);
}

/*
 * This function grabs your pointer and lets you drag stuff around (borders).
 * Every time you move your mouse, an XCB_MOTION_NOTIFY event will be received
 * and the given callback will be called with the parameters specified (client,
 * border on which the click originally was), the original rect of the client,
 * the event and the new coordinates (x, y).
 *
 */
void drag_pointer(Con *con, xcb_button_press_event_t *event, xcb_window_t
                confine_to, border_t border, callback_t callback, void *extra)
{
    uint32_t new_x, new_y;
    Rect old_rect;
    if (con != NULL)
        memcpy(&old_rect, &(con->rect), sizeof(Rect));

    /* Grab the pointer */
    /* TODO: returncode */
    xcb_grab_pointer(conn, 
                     false,               /* get all pointer events specified by the following mask */
                     root,                /* grab the root window */
                     XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, /* which events to let through */
                     XCB_GRAB_MODE_ASYNC, /* pointer events should continue as normal */
                     XCB_GRAB_MODE_ASYNC, /* keyboard mode */
                     confine_to,          /* confine_to = in which window should the cursor stay */
                     XCB_NONE,            /* don’t display a special cursor */
                     XCB_CURRENT_TIME);

    /* Go into our own event loop */
    xcb_flush(conn);

    xcb_generic_event_t *inside_event, *last_motion_notify = NULL;
    /* I’ve always wanted to have my own eventhandler… */
    while ((inside_event = xcb_wait_for_event(conn))) {
        /* We now handle all events we can get using xcb_poll_for_event */
        do {
            /* skip x11 errors */
            if (inside_event->response_type == 0) {
                free(inside_event);
                continue;
            }
            /* Strip off the highest bit (set if the event is generated) */
            int type = (inside_event->response_type & 0x7F);

            switch (type) {
                case XCB_BUTTON_RELEASE:
                    goto done;

                case XCB_MOTION_NOTIFY:
                    /* motion_notify events are saved for later */
                    FREE(last_motion_notify);
                    last_motion_notify = inside_event;
                    break;

                case XCB_UNMAP_NOTIFY:
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE:
                    DLOG("Unmap-notify, aborting\n");
                    handle_event(type, inside_event);
                    goto done;

                default:
                    DLOG("Passing to original handler\n");
                    /* Use original handler */
                    handle_event(type, inside_event);
                    break;
            }
            if (last_motion_notify != inside_event)
                free(inside_event);
        } while ((inside_event = xcb_poll_for_event(conn)) != NULL);

        if (last_motion_notify == NULL)
            continue;

        new_x = ((xcb_motion_notify_event_t*)last_motion_notify)->root_x;
        new_y = ((xcb_motion_notify_event_t*)last_motion_notify)->root_y;

        callback(con, &old_rect, new_x, new_y, extra);
        FREE(last_motion_notify);
    }
done:
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_flush(conn);
}

#if 0
/*
 * Changes focus in the given direction for floating clients.
 *
 * Changing to the left/right means going to the previous/next floating client,
 * changing to top/bottom means cycling through the Z-index.
 *
 */
void floating_focus_direction(xcb_connection_t *conn, Client *currently_focused, direction_t direction) {
        DLOG("floating focus\n");

        if (direction == D_LEFT || direction == D_RIGHT) {
                /* Go to the next/previous floating client */
                Client *client;

                while ((client = (direction == D_LEFT ? TAILQ_PREV(currently_focused, floating_clients_head, floating_clients) :
                                                        TAILQ_NEXT(currently_focused, floating_clients))) !=
                       TAILQ_END(&(currently_focused->workspace->floating_clients))) {
                        if (!client->floating)
                                continue;
                        set_focus(conn, client, true);
                        return;
                }
        }
}

/*
 * Moves the client 10px to the specified direction.
 *
 */
void floating_move(xcb_connection_t *conn, Client *currently_focused, direction_t direction) {
        DLOG("floating move\n");

        Rect destination = currently_focused->rect;
        Rect *screen = &(currently_focused->workspace->output->rect);

        switch (direction) {
                case D_LEFT:
                        destination.x -= 10;
                        break;
                case D_RIGHT:
                        destination.x += 10;
                        break;
                case D_UP:
                        destination.y -= 10;
                        break;
                case D_DOWN:
                        destination.y += 10;
                        break;
                /* to make static analyzers happy */
                default:
                        break;
        }

        /* Prevent windows from vanishing completely */
        if ((int32_t)(destination.x + destination.width - 5) <= (int32_t)screen->x ||
            (int32_t)(destination.x + 5) >= (int32_t)(screen->x + screen->width) ||
            (int32_t)(destination.y + destination.height - 5) <= (int32_t)screen->y ||
            (int32_t)(destination.y + 5) >= (int32_t)(screen->y + screen->height)) {
                DLOG("boundary check failed, not moving\n");
                return;
        }

        currently_focused->rect = destination;
        reposition_client(conn, currently_focused);

        /* Because reposition_client does not send a faked configure event (only resize does),
         * we need to initiate that on our own */
        fake_absolute_configure_notify(conn, currently_focused);
        /* fake_absolute_configure_notify flushes */
}

/*
 * Hides all floating clients (or show them if they are currently hidden) on
 * the specified workspace.
 *
 */
void floating_toggle_hide(xcb_connection_t *conn, Workspace *workspace) {
        Client *client;

        workspace->floating_hidden = !workspace->floating_hidden;
        DLOG("floating_hidden is now: %d\n", workspace->floating_hidden);
        TAILQ_FOREACH(client, &(workspace->floating_clients), floating_clients) {
                if (workspace->floating_hidden)
                        client_unmap(conn, client);
                else client_map(conn, client);
        }

        /* If we just unmapped all floating windows we should ensure that the focus
         * is set correctly, that ist, to the first non-floating client in stack */
        if (workspace->floating_hidden)
                SLIST_FOREACH(client, &(workspace->focus_stack), focus_clients) {
                        if (client_is_floating(client))
                                continue;
                        set_focus(conn, client, true);
                        return;
                }

        xcb_flush(conn);
}
#endif
