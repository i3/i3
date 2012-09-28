#undef I3__FILE__
#define I3__FILE__ "floating.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * floating.c: Floating windows.
 *
 */
#include "all.h"

extern xcb_connection_t *conn;

/*
 * Calculates sum of heights and sum of widths of all currently active outputs
 *
 */
static Rect total_outputs_dimensions(void) {
    Output *output;
    /* Use Rect to encapsulate dimensions, ignoring x/y */
    Rect outputs_dimensions = {0, 0, 0, 0};
    TAILQ_FOREACH(output, &outputs, outputs) {
        outputs_dimensions.height += output->rect.height;
        outputs_dimensions.width += output->rect.width;
    }
    return outputs_dimensions;
}

void floating_enable(Con *con, bool automatic) {
    bool set_focus = (con == focused);

    if (con->parent && con->parent->type == CT_DOCKAREA) {
        LOG("Container is a dock window, not enabling floating mode.\n");
        return;
    }

    if (con_is_floating(con)) {
        LOG("Container is already in floating mode, not doing anything.\n");
        return;
    }

    /* 1: If the container is a workspace container, we need to create a new
     * split-container with the same layout and make that one floating. We
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
        new->layout = con->layout;

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
    Con *ws = con_get_workspace(con);
    nc->parent = ws;
    nc->split = true;
    nc->type = CT_FLOATING_CON;
    nc->layout = L_SPLITH;
    /* We insert nc already, even though its rect is not yet calculated. This
     * is necessary because otherwise the workspace might be empty (and get
     * closed in tree_close()) even though it’s not. */
    TAILQ_INSERT_TAIL(&(ws->floating_head), nc, floating_windows);
    TAILQ_INSERT_TAIL(&(ws->focus_head), nc, focused);

    /* check if the parent container is empty and close it if so */
    if ((con->parent->type == CT_CON || con->parent->type == CT_FLOATING_CON) &&
        con_num_children(con->parent) == 0) {
        DLOG("Old container empty after setting this child to floating, closing\n");
        tree_close(con->parent, DONT_KILL_WINDOW, false, false);
    }

    char *name;
    sasprintf(&name, "[i3 con] floatingcon around %p", con);
    x_set_name(nc, name);
    free(name);

    /* find the height for the decorations */
    int deco_height = config.font.height + 5;

    DLOG("Original rect: (%d, %d) with %d x %d\n", con->rect.x, con->rect.y, con->rect.width, con->rect.height);
    DLOG("Geometry = (%d, %d) with %d x %d\n", con->geometry.x, con->geometry.y, con->geometry.width, con->geometry.height);
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

    /* Define reasonable minimal and maximal sizes for floating windows */
    const int floating_sane_min_height = 50;
    const int floating_sane_min_width = 75;

    Rect floating_sane_max_dimensions;
    floating_sane_max_dimensions = total_outputs_dimensions();

    /* Unless user requests otherwise (-1), ensure width/height do not exceed
     * configured maxima or, if unconfigured, limit to combined width of all
     * outputs */
    if (config.floating_maximum_height != -1) {
        if (config.floating_maximum_height == 0)
            nc->rect.height = min(nc->rect.height, floating_sane_max_dimensions.height);
        else
            nc->rect.height = min(nc->rect.height, config.floating_maximum_height);
    }
    if (config.floating_maximum_width != -1) {
        if (config.floating_maximum_width == 0)
            nc->rect.width = min(nc->rect.width, floating_sane_max_dimensions.width);
        else
            nc->rect.width = min(nc->rect.width, config.floating_maximum_width);
    }

    /* Unless user requests otherwise (-1), raise the width/height to
     * reasonable minimum dimensions */
    if (config.floating_minimum_height != -1) {
        if (config.floating_minimum_height == 0)
            nc->rect.height = max(nc->rect.height, floating_sane_min_height);
        else
            nc->rect.height = max(nc->rect.height, config.floating_minimum_height);
    }
    if (config.floating_minimum_width != -1) {
        if (config.floating_minimum_width == 0)
            nc->rect.width = max(nc->rect.width, floating_sane_min_width);
        else
            nc->rect.width = max(nc->rect.width, config.floating_minimum_width);
    }

    /* 3: attach the child to the new parent container. We need to do this
     * because con_border_style_rect() needs to access con->parent. */
    con->parent = nc;
    con->percent = 1.0;
    con->floating = FLOATING_USER_ON;

    /* 4: set the border style as specified with new_float */
    if (automatic)
        con->border_style = config.default_floating_border;

    /* Add pixels for the decoration. */
    Rect border_style_rect = con_border_style_rect(con);

    nc->rect.height -= border_style_rect.height;
    nc->rect.width -= border_style_rect.width;

    /* Add some more pixels for the title bar */
    if(con_border_style(con) == BS_NORMAL)
        nc->rect.height += deco_height;

    /* Honor the X11 border */
    nc->rect.height += con->border_width * 2;
    nc->rect.width += con->border_width * 2;

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
            nc->rect.x = ws->rect.x + (ws->rect.width / 2) - (nc->rect.width / 2);
            nc->rect.y = ws->rect.y + (ws->rect.height / 2) - (nc->rect.height / 2);
        }
    }

    /* Sanity check: Are the coordinates on the appropriate output? If not, we
     * need to change them */
    Output *current_output = get_output_containing(nc->rect.x, nc->rect.y);
    Con *correct_output = con_get_output(ws);
    if (!current_output || current_output->con != correct_output) {
        DLOG("This floating window is on the wrong output, fixing coordinates (currently (%d, %d))\n",
             nc->rect.x, nc->rect.y);
        /* Take the relative coordinates of the current output, then add them
         * to the coordinate space of the correct output */
        uint32_t rel_x = (nc->rect.x - (current_output ? current_output->con->rect.x : 0));
        uint32_t rel_y = (nc->rect.y - (current_output ? current_output->con->rect.y : 0));
        nc->rect.x = correct_output->rect.x + rel_x;
        nc->rect.y = correct_output->rect.y + rel_y;
    }

    DLOG("Floating rect: (%d, %d) with %d x %d\n", nc->rect.x, nc->rect.y, nc->rect.width, nc->rect.height);

    /* 5: Subtract the deco_height in order to make the floating window appear
     * at precisely the position it specified in its original geometry (which
     * is what applications might remember). */
    deco_height = (con->border_style == BS_NORMAL ? config.font.height + 5 : 0);
    nc->rect.y -= deco_height;

    DLOG("Corrected y = %d (deco_height = %d)\n", nc->rect.y, deco_height);

    TAILQ_INSERT_TAIL(&(nc->nodes_head), con, nodes);
    TAILQ_INSERT_TAIL(&(nc->focus_head), con, focused);

    /* render the cons to get initial window_rect correct */
    render_con(nc, false);
    render_con(con, false);

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
    tree_close(con->parent, DONT_KILL_WINDOW, true, false);

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

    con->floating = FLOATING_USER_OFF;

    con_attach(con, con->parent, false);

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
    con_move_to_workspace(con, ws, false, true);
    con_focus(con_descend_focused(con));
    return true;
}

DRAGGING_CB(drag_window_callback) {
    const struct xcb_button_press_event_t *event = extra;

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
void floating_drag_window(Con *con, const xcb_button_press_event_t *event) {
    DLOG("floating_drag_window\n");

    /* Push changes before dragging, so that the window gets raised now and not
     * after the user releases the mouse button */
    tree_render();

    /* Drag the window */
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
    const border_t corner;
    const bool proportional;
    const xcb_button_press_event_t *event;
};

DRAGGING_CB(resize_window_callback) {
    const struct resize_window_callback_params *params = extra;
    const xcb_button_press_event_t *event = params->event;
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
void floating_resize_window(Con *con, const bool proportional,
                            const xcb_button_press_event_t *event) {
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
void drag_pointer(Con *con, const xcb_button_press_event_t *event, xcb_window_t
                confine_to, border_t border, callback_t callback, const void *extra)
{
    uint32_t new_x, new_y;
    Rect old_rect = { 0, 0, 0, 0 };
    if (con != NULL)
        memcpy(&old_rect, &(con->rect), sizeof(Rect));

    /* Grab the pointer */
    xcb_grab_pointer_cookie_t cookie;
    xcb_grab_pointer_reply_t *reply;
    cookie = xcb_grab_pointer(conn,
        false,               /* get all pointer events specified by the following mask */
        root,                /* grab the root window */
        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, /* which events to let through */
        XCB_GRAB_MODE_ASYNC, /* pointer events should continue as normal */
        XCB_GRAB_MODE_ASYNC, /* keyboard mode */
        confine_to,          /* confine_to = in which window should the cursor stay */
        XCB_NONE,            /* don’t display a special cursor */
        XCB_CURRENT_TIME);

    if ((reply = xcb_grab_pointer_reply(conn, cookie, NULL)) == NULL) {
        ELOG("Could not grab pointer\n");
        return;
    }

    free(reply);

    /* Go into our own event loop */
    xcb_flush(conn);

    xcb_generic_event_t *inside_event, *last_motion_notify = NULL;
    bool loop_done = false;
    /* I’ve always wanted to have my own eventhandler… */
    while (!loop_done && (inside_event = xcb_wait_for_event(conn))) {
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
                    loop_done = true;
                    break;

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
                    loop_done = true;
                    break;

                default:
                    DLOG("Passing to original handler\n");
                    /* Use original handler */
                    handle_event(type, inside_event);
                    break;
            }
            if (last_motion_notify != inside_event)
                free(inside_event);
        } while ((inside_event = xcb_poll_for_event(conn)) != NULL);

        if (last_motion_notify == NULL || loop_done)
            continue;

        new_x = ((xcb_motion_notify_event_t*)last_motion_notify)->root_x;
        new_y = ((xcb_motion_notify_event_t*)last_motion_notify)->root_y;

        callback(con, &old_rect, new_x, new_y, extra);
        FREE(last_motion_notify);
    }

    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_flush(conn);
}

/*
 * Repositions the CT_FLOATING_CON to have the coordinates specified by
 * newrect, but only if the coordinates are not out-of-bounds. Also reassigns
 * the floating con to a different workspace if this move was across different
 * outputs.
 *
 */
void floating_reposition(Con *con, Rect newrect) {
    /* Sanity check: Are the new coordinates on any output? If not, we
     * ignore that request. */
    Output *output = get_output_containing(
        newrect.x + (newrect.width / 2),
        newrect.y + (newrect.height / 2));

    if (!output) {
        ELOG("No output found at destination coordinates. Not repositioning.\n");
        return;
    }

    con->rect = newrect;

    floating_maybe_reassign_ws(con);
    tree_render();
}

/*
 * Fixes the coordinates of the floating window whenever the window gets
 * reassigned to a different output (or when the output’s rect changes).
 *
 */
void floating_fix_coordinates(Con *con, Rect *old_rect, Rect *new_rect) {
    DLOG("Fixing coordinates of floating window %p (rect (%d, %d), %d x %d)\n",
         con, con->rect.x, con->rect.y, con->rect.width, con->rect.height);
    DLOG("old_rect = (%d, %d), %d x %d\n",
         old_rect->x, old_rect->y, old_rect->width, old_rect->height);
    DLOG("new_rect = (%d, %d), %d x %d\n",
         new_rect->x, new_rect->y, new_rect->width, new_rect->height);
    /* First we get the x/y coordinates relative to the x/y coordinates
     * of the output on which the window is on */
    int32_t rel_x = (con->rect.x - old_rect->x);
    int32_t rel_y = (con->rect.y - old_rect->y);
    /* Then we calculate a fraction, for example 0.63 for a window
     * which is at y = 1212 of a 1920 px high output */
    DLOG("rel_x = %d, rel_y = %d, fraction_x = %f, fraction_y = %f, output->w = %d, output->h = %d\n",
          rel_x, rel_y, (double)rel_x / old_rect->width, (double)rel_y / old_rect->height,
          old_rect->width, old_rect->height);
    /* Here we have to multiply at first. Or we will lose precision when not compiled with -msse2 */
    con->rect.x = (int32_t)new_rect->x + (double)(rel_x * (int32_t)new_rect->width)  / (int32_t)old_rect->width;
    con->rect.y = (int32_t)new_rect->y + (double)(rel_y * (int32_t)new_rect->height) / (int32_t)old_rect->height;
    DLOG("Resulting coordinates: x = %d, y = %d\n", con->rect.x, con->rect.y);
}

#if 0
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
