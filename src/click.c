#undef I3__FILE__
#define I3__FILE__ "click.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * click.c: Button press (mouse click) events.
 *
 */
#include "all.h"

#include <time.h>
#include <math.h>

#include <xcb/xcb_icccm.h>

#include <X11/XKBlib.h>

typedef enum { CLICK_BORDER = 0,
               CLICK_DECORATION = 1,
               CLICK_INSIDE = 2 } click_destination_t;

/*
 * Finds the correct pair of first/second cons between the resize will take
 * place according to the passed border position (top, left, right, bottom),
 * then calls resize_graphical_handler().
 *
 */
static bool tiling_resize_for_border(Con *con, border_t border, xcb_button_press_event_t *event) {
    DLOG("border = %d, con = %p\n", border, con);
    Con *second = NULL;
    Con *first = con;
    direction_t search_direction;
    switch (border) {
        case BORDER_LEFT:
            search_direction = D_LEFT;
            break;
        case BORDER_RIGHT:
            search_direction = D_RIGHT;
            break;
        case BORDER_TOP:
            search_direction = D_UP;
            break;
        case BORDER_BOTTOM:
            search_direction = D_DOWN;
            break;
    }

    bool res = resize_find_tiling_participants(&first, &second, search_direction);
    if (!res) {
        LOG("No second container in this direction found.\n");
        return false;
    }

    assert(first != second);
    assert(first->parent == second->parent);

    /* The first container should always be in front of the second container */
    if (search_direction == D_UP || search_direction == D_LEFT) {
        Con *tmp = first;
        first = second;
        second = tmp;
    }

    const orientation_t orientation = ((border == BORDER_LEFT || border == BORDER_RIGHT) ? HORIZ : VERT);

    resize_graphical_handler(first, second, orientation, event);

    DLOG("After resize handler, rendering\n");
    tree_render();
    return true;
}

/*
 * Called when the user clicks using the floating_modifier, but the client is in
 * tiling layout.
 *
 * Returns false if it does not do anything (that is, the click should be sent
 * to the client).
 *
 */
static bool floating_mod_on_tiled_client(Con *con, xcb_button_press_event_t *event) {
    /* The client is in tiling layout. We can still initiate a resize with the
     * right mouse button, by chosing the border which is the most near one to
     * the position of the mouse pointer */
    int to_right = con->rect.width - event->event_x,
        to_left = event->event_x,
        to_top = event->event_y,
        to_bottom = con->rect.height - event->event_y;

    DLOG("click was %d px to the right, %d px to the left, %d px to top, %d px to bottom\n",
         to_right, to_left, to_top, to_bottom);

    if (to_right < to_left &&
        to_right < to_top &&
        to_right < to_bottom)
        return tiling_resize_for_border(con, BORDER_RIGHT, event);

    if (to_left < to_right &&
        to_left < to_top &&
        to_left < to_bottom)
        return tiling_resize_for_border(con, BORDER_LEFT, event);

    if (to_top < to_right &&
        to_top < to_left &&
        to_top < to_bottom)
        return tiling_resize_for_border(con, BORDER_TOP, event);

    if (to_bottom < to_right &&
        to_bottom < to_left &&
        to_bottom < to_top)
        return tiling_resize_for_border(con, BORDER_BOTTOM, event);

    return false;
}

/*
 * Finds out which border was clicked on and calls tiling_resize_for_border().
 *
 */
static bool tiling_resize(Con *con, xcb_button_press_event_t *event, const click_destination_t dest) {
    /* check if this was a click on the window border (and on which one) */
    Rect bsr = con_border_style_rect(con);
    DLOG("BORDER x = %d, y = %d for con %p, window 0x%08x\n",
         event->event_x, event->event_y, con, event->event);
    DLOG("checks for right >= %d\n", con->window_rect.x + con->window_rect.width);
    if (dest == CLICK_DECORATION) {
        /* The user clicked on a window decoration. We ignore the following case:
         * The container is a h-split, tabbed or stacked container with > 1
         * window. Decorations will end up next to each other and the user
         * expects to switch to a window by clicking on its decoration. */

        /* Since the container might either be the child *or* already a split
         * container (in the case of a nested split container), we need to make
         * sure that we are dealing with the split container here. */
        Con *check_con = con;
        if (con_is_leaf(check_con) && check_con->parent->type == CT_CON)
            check_con = check_con->parent;

        if ((check_con->layout == L_STACKED ||
             check_con->layout == L_TABBED ||
             con_orientation(check_con) == HORIZ) &&
            con_num_children(check_con) > 1) {
            DLOG("Not handling this resize, this container has > 1 child.\n");
            return false;
        }
        return tiling_resize_for_border(con, BORDER_TOP, event);
    }

    if (event->event_x >= 0 && event->event_x <= (int32_t)bsr.x &&
        event->event_y >= (int32_t)bsr.y && event->event_y <= (int32_t)(con->rect.height + bsr.height))
        return tiling_resize_for_border(con, BORDER_LEFT, event);

    if (event->event_x >= (int32_t)(con->window_rect.x + con->window_rect.width) &&
        event->event_y >= (int32_t)bsr.y && event->event_y <= (int32_t)(con->rect.height + bsr.height))
        return tiling_resize_for_border(con, BORDER_RIGHT, event);

    if (event->event_y >= (int32_t)(con->window_rect.y + con->window_rect.height))
        return tiling_resize_for_border(con, BORDER_BOTTOM, event);

    return false;
}

/*
 * Being called by handle_button_press, this function calls the appropriate
 * functions for resizing/dragging.
 *
 */
static int route_click(Con *con, xcb_button_press_event_t *event, const bool mod_pressed, const click_destination_t dest) {
    DLOG("--> click properties: mod = %d, destination = %d\n", mod_pressed, dest);
    DLOG("--> OUTCOME = %p\n", con);
    DLOG("type = %d, name = %s\n", con->type, con->name);

    /* don’t handle dockarea cons, they must not be focused */
    if (con->parent->type == CT_DOCKAREA)
        goto done;

    /* if the user has bound an action to this click, it should override the
     * default behavior. */
    if (dest == CLICK_DECORATION || dest == CLICK_INSIDE) {
        Binding *bind = get_binding_from_xcb_event((xcb_generic_event_t *)event);
        /* clicks over a window decoration will always trigger the binding and
         * clicks on the inside of the window will only trigger a binding if
         * the --whole-window flag was given for the binding. */
        if (bind && (dest == CLICK_DECORATION || bind->whole_window)) {
            CommandResult *result = run_binding(bind, con);

            /* ASYNC_POINTER eats the event */
            xcb_allow_events(conn, XCB_ALLOW_ASYNC_POINTER, event->time);
            xcb_flush(conn);

            if (result->needs_tree_render)
                tree_render();

            command_result_free(result);

            return 0;
        }
    }

    /* There is no default behavior for button release events so we are done. */
    if (event->response_type == XCB_BUTTON_RELEASE) {
        goto done;
    }

    /* Any click in a workspace should focus that workspace. If the
     * workspace is on another output we need to do a workspace_show in
     * order for i3bar (and others) to notice the change in workspace. */
    Con *ws = con_get_workspace(con);
    Con *focused_workspace = con_get_workspace(focused);

    if (!ws) {
        ws = TAILQ_FIRST(&(output_get_content(con_get_output(con))->focus_head));
        if (!ws)
            goto done;
    }

    if (ws != focused_workspace)
        workspace_show(ws);

    /* get the floating con */
    Con *floatingcon = con_inside_floating(con);
    const bool proportional = (event->state & BIND_SHIFT);
    const bool in_stacked = (con->parent->layout == L_STACKED || con->parent->layout == L_TABBED);

    /* 1: see if the user scrolled on the decoration of a stacked/tabbed con */
    if (in_stacked &&
        dest == CLICK_DECORATION &&
        (event->detail == XCB_BUTTON_INDEX_4 ||
         event->detail == XCB_BUTTON_INDEX_5)) {
        DLOG("Scrolling on a window decoration\n");
        orientation_t orientation = (con->parent->layout == L_STACKED ? VERT : HORIZ);
        /* Focus the currently focused container on the same level that the
         * user scrolled on. e.g. the tabbed decoration contains
         * "urxvt | i3: V[xterm geeqie] | firefox",
         * focus is on the xterm, but the user scrolled on urxvt.
         * The splitv container will be focused. */
        Con *focused = con->parent;
        focused = TAILQ_FIRST(&(focused->focus_head));
        con_focus(focused);
        /* To prevent scrolling from going outside the container (see ticket
         * #557), we first check if scrolling is possible at all. */
        bool scroll_prev_possible = (TAILQ_PREV(focused, nodes_head, nodes) != NULL);
        bool scroll_next_possible = (TAILQ_NEXT(focused, nodes) != NULL);
        if (event->detail == XCB_BUTTON_INDEX_4 && scroll_prev_possible)
            tree_next('p', orientation);
        else if (event->detail == XCB_BUTTON_INDEX_5 && scroll_next_possible)
            tree_next('n', orientation);
        goto done;
    }

    /* 2: focus this con. */
    con_focus(con);

    /* 3: For floating containers, we also want to raise them on click.
     * We will skip handling events on floating cons in fullscreen mode */
    Con *fs = (ws ? con_get_fullscreen_con(ws, CF_OUTPUT) : NULL);
    if (floatingcon != NULL && fs != con) {
        floating_raise_con(floatingcon);

        /* 4: floating_modifier plus left mouse button drags */
        if (mod_pressed && event->detail == XCB_BUTTON_INDEX_1) {
            floating_drag_window(floatingcon, event);
            return 1;
        }

        /*  5: resize (floating) if this was a (left or right) click on the
         * left/right/bottom border, or a right click on the decoration.
         * also try resizing (tiling) if it was a click on the top */
        if (mod_pressed && event->detail == XCB_BUTTON_INDEX_3) {
            DLOG("floating resize due to floatingmodifier\n");
            floating_resize_window(floatingcon, proportional, event);
            return 1;
        }

        if (!in_stacked && dest == CLICK_DECORATION) {
            /* try tiling resize, but continue if it doesn’t work */
            DLOG("tiling resize with fallback\n");
            if (tiling_resize(con, event, dest))
                goto done;
        }

        if (dest == CLICK_DECORATION && event->detail == XCB_BUTTON_INDEX_3) {
            DLOG("floating resize due to decoration right click\n");
            floating_resize_window(floatingcon, proportional, event);
            return 1;
        }

        if (dest == CLICK_BORDER) {
            DLOG("floating resize due to border click\n");
            floating_resize_window(floatingcon, proportional, event);
            return 1;
        }

        /* 6: dragging, if this was a click on a decoration (which did not lead
         * to a resize) */
        if (!in_stacked && dest == CLICK_DECORATION) {
            floating_drag_window(floatingcon, event);
            return 1;
        }

        goto done;
    }

    if (in_stacked) {
        /* for stacked/tabbed cons, the resizing applies to the parent
         * container */
        con = con->parent;
    }

    /* 7: floating modifier pressed, initiate a resize */
    if (dest == CLICK_INSIDE && mod_pressed && event->detail == XCB_BUTTON_INDEX_3) {
        if (floating_mod_on_tiled_client(con, event))
            return 1;
    }
    /* 8: otherwise, check for border/decoration clicks and resize */
    else if ((dest == CLICK_BORDER || dest == CLICK_DECORATION) &&
             (event->detail == XCB_BUTTON_INDEX_1 ||
              event->detail == XCB_BUTTON_INDEX_3)) {
        DLOG("Trying to resize (tiling)\n");
        tiling_resize(con, event, dest);
    }

done:
    xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
    xcb_flush(conn);
    tree_render();

    return 0;
}

/*
 * The button press X callback. This function determines whether the floating
 * modifier is pressed and where the user clicked (decoration, border, inside
 * the window).
 *
 * Then, route_click is called on the appropriate con.
 *
 */
int handle_button_press(xcb_button_press_event_t *event) {
    Con *con;
    DLOG("Button %d %s on window 0x%08x (child 0x%08x) at (%d, %d) (root %d, %d)\n",
         event->state, (event->response_type == XCB_BUTTON_PRESS ? "press" : "release"),
         event->event, event->child, event->event_x, event->event_y, event->root_x,
         event->root_y);

    last_timestamp = event->time;

    const uint32_t mod = config.floating_modifier;
    const bool mod_pressed = (mod != 0 && (event->state & mod) == mod);
    DLOG("floating_mod = %d, detail = %d\n", mod_pressed, event->detail);
    if ((con = con_by_window_id(event->event)))
        return route_click(con, event, mod_pressed, CLICK_INSIDE);

    if (!(con = con_by_frame_id(event->event))) {
        /* If the root window is clicked, find the relevant output from the
         * click coordinates and focus the output's active workspace. */
        if (event->event == root && event->response_type == XCB_BUTTON_PRESS) {
            Con *output, *ws;
            TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
                if (con_is_internal(output) ||
                    !rect_contains(output->rect, event->event_x, event->event_y))
                    continue;

                ws = TAILQ_FIRST(&(output_get_content(output)->focus_head));
                if (ws != con_get_workspace(focused)) {
                    workspace_show(ws);
                    tree_render();
                }
                return 1;
            }
            return 0;
        }

        ELOG("Clicked into unknown window?!\n");
        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
        xcb_flush(conn);
        return 0;
    }

    if (event->child != XCB_NONE) {
        DLOG("event->child not XCB_NONE, so this is an event which originated from a click into the application, but the application did not handle it.\n");
        return route_click(con, event, mod_pressed, CLICK_INSIDE);
    }

    /* Check if the click was on the decoration of a child */
    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        if (!rect_contains(child->deco_rect, event->event_x, event->event_y))
            continue;

        return route_click(child, event, mod_pressed, CLICK_DECORATION);
    }

    return route_click(con, event, mod_pressed, CLICK_BORDER);
}
