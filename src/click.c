/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * click.c: Button press (mouse click) events.
 *
 */
#include "all.h"

#include <time.h>
#include <math.h>

#include <xcb/xcb_icccm.h>

#include <X11/XKBlib.h>

/*
 * Finds the correct pair of first/second cons between the resize will take
 * place according to the passed border position (top, left, right, bottom),
 * then calls resize_graphical_handler().
 *
 */
static bool tiling_resize_for_border(Con *con, border_t border, xcb_button_press_event_t *event, bool use_threshold, bool both) {
    DLOG("border = %d, con = %p\n", border, con);

    Con *first_h = NULL;
    Con *first_v = NULL;
    Con *second_h = NULL;
    Con *second_v = NULL;

    if (border & BORDER_LEFT) {
        first_h = con;
        resize_find_tiling_participants(&first_h, &second_h, D_LEFT, false);
        /* The first container should always be in front of the second container */
        SWAP(first_h, second_h, Con *);
    } else if (border & BORDER_RIGHT) {
        first_h = con;
        resize_find_tiling_participants(&first_h, &second_h, D_RIGHT, false);
    }
    if (border & BORDER_TOP) {
        first_v = con;
        resize_find_tiling_participants(&first_v, &second_v, D_UP, false);
        /* The first container should always be in front of the second container */
        SWAP(first_v, second_v, Con *);
    } else if (border & BORDER_BOTTOM) {
        first_v = con;
        resize_find_tiling_participants(&first_v, &second_v, D_DOWN, false);
    }

    if (first_h != NULL && first_h->fullscreen_mode != second_h->fullscreen_mode) {
        DLOG("Avoiding horisontal resize between containers with different fullscreen modes, %d != %d\n",
             first_h->fullscreen_mode, second_h->fullscreen_mode);
        first_h = second_h = NULL;
    }
    if (first_v != NULL && first_v->fullscreen_mode != second_v->fullscreen_mode) {
        DLOG("Avoiding vertical resize between containers with different fullscreen modes, %d != %d\n",
             first_v->fullscreen_mode, second_v->fullscreen_mode);
        first_v = second_v = NULL;
    }

    if (!first_h && !first_v) {
        DLOG("No directions to resize found.\n");
        return false;
    }

    border &=
        (first_v ? BORDER_TOP | BORDER_BOTTOM : 0) |
        (first_h ? BORDER_LEFT | BORDER_RIGHT : 0);

    resize_graphical_handler(event, resize_cursor(border, both), use_threshold,
                             con_get_output(con), first_h, first_v, second_h, second_v);

    return true;
}

/*
 * Called when the user clicks using the floating_modifier, but the client is in
 * tiling layout.
 *
 */
static void floating_mod_on_tiled_client(Con *con, xcb_button_press_event_t *event) {
    border_t border = resize_get_borders_mod(event->event_x, event->event_y,
                                             con->window_rect.width, con->window_rect.height);
    bool done = tiling_resize_for_border(con, border, event, false, false);
    if (!done) {
        /* Just grab the cursor and do nothing. */
        resize_graphical_handler(event, XCURSOR_CURSOR_NOT_ALLOWED, false,
                                 con_get_output(con), NULL, NULL, NULL, NULL);
    }
}

/*
 * Finds out which border was clicked on and calls tiling_resize_for_border().
 *
 */
static bool tiling_resize(Con *con, xcb_button_press_event_t *event, click_destination_t dest, bool use_threshold) {
    border_t border = resize_get_borders_sides(con, event->root_x, event->root_y, dest);
    return tiling_resize_for_border(con, border, event, use_threshold, true);
}

/*
 * Being called by handle_button_press, this function calls the appropriate
 * functions for resizing/dragging.
 *
 */
static void route_click(Con *con, xcb_button_press_event_t *event, const bool mod_pressed, const click_destination_t dest) {
    DLOG("--> click properties: mod = %d, destination = %d\n", mod_pressed, dest);
    DLOG("--> OUTCOME = %p\n", con);
    DLOG("type = %d, name = %s\n", con->type, con->name);

    /* don’t handle dockarea cons, they must not be focused */
    if (con->parent->type == CT_DOCKAREA)
        goto done;

    const bool is_left_or_right_click = (event->detail == XCB_BUTTON_CLICK_LEFT ||
                                         event->detail == XCB_BUTTON_CLICK_RIGHT);

    /* if the user has bound an action to this click, it should override the
     * default behavior. */
    if (dest == CLICK_DECORATION || dest == CLICK_INSIDE || dest == CLICK_BORDER) {
        Binding *bind = get_binding_from_xcb_event((xcb_generic_event_t *)event);

        if (bind != NULL && ((dest == CLICK_DECORATION && !bind->exclude_titlebar) ||
                             (dest == CLICK_INSIDE && bind->whole_window) ||
                             (dest == CLICK_BORDER && bind->border))) {
            CommandResult *result = run_binding(bind, con);

            /* ASYNC_POINTER eats the event */
            xcb_allow_events(conn, XCB_ALLOW_ASYNC_POINTER, event->time);
            xcb_flush(conn);

            command_result_free(result);
            return;
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
    const bool proportional = (event->state & XCB_KEY_BUT_MASK_SHIFT) == XCB_KEY_BUT_MASK_SHIFT;
    const bool in_stacked = (con->parent->layout == L_STACKED || con->parent->layout == L_TABBED);
    const bool was_focused = focused == con;

    /* 1: see if the user scrolled on the decoration of a stacked/tabbed con */
    if (in_stacked &&
        dest == CLICK_DECORATION &&
        (event->detail == XCB_BUTTON_SCROLL_UP ||
         event->detail == XCB_BUTTON_SCROLL_DOWN ||
         event->detail == XCB_BUTTON_SCROLL_LEFT ||
         event->detail == XCB_BUTTON_SCROLL_RIGHT)) {
        DLOG("Scrolling on a window decoration\n");
        /* Use the focused child of the tabbed / stacked container, not the
         * container the user scrolled on. */
        Con *current = TAILQ_FIRST(&(con->parent->focus_head));
        const position_t direction =
            (event->detail == XCB_BUTTON_SCROLL_UP || event->detail == XCB_BUTTON_SCROLL_LEFT) ? BEFORE : AFTER;
        Con *next = get_tree_next_sibling(current, direction);
        con_activate(con_descend_focused(next ? next : current));

        goto done;
    }

    /* 2: focus this con. */
    con_activate(con);

    /* 3: For floating containers, we also want to raise them on click.
     * We will skip handling events on floating cons in fullscreen mode */
    Con *fs = con_get_fullscreen_covering_ws(ws);
    if (floatingcon != NULL && fs != con) {
        /* 4: floating_modifier plus left mouse button drags */
        if (mod_pressed && event->detail == XCB_BUTTON_CLICK_LEFT) {
            floating_drag_window(floatingcon, event, false);
            return;
        }

        /*  5: resize (floating) if this was a (left or right) click on the
         * left/right/bottom border, or a right click on the decoration.
         * also try resizing (tiling) if possible */
        if (mod_pressed && event->detail == XCB_BUTTON_CLICK_RIGHT) {
            DLOG("floating resize due to floatingmodifier\n");
            floating_resize_window(floatingcon, proportional, event);
            return;
        }

        if ((dest == CLICK_BORDER || dest == CLICK_DECORATION) &&
            is_left_or_right_click) {
            /* try tiling resize, but continue if it doesn’t work */
            DLOG("tiling resize with fallback\n");
            if (tiling_resize(con, event, dest, dest == CLICK_DECORATION && !was_focused))
                goto done;
        }

        if (dest == CLICK_DECORATION && event->detail == XCB_BUTTON_CLICK_RIGHT) {
            DLOG("floating resize due to decoration right click\n");
            floating_resize_window(floatingcon, proportional, event);
            return;
        }

        if (dest == CLICK_BORDER && is_left_or_right_click) {
            DLOG("floating resize due to border click\n");
            floating_resize_window(floatingcon, proportional, event);
            return;
        }

        /* 6: dragging, if this was a click on a decoration (which did not lead
         * to a resize) */
        if (dest == CLICK_DECORATION && event->detail == XCB_BUTTON_CLICK_LEFT) {
            floating_drag_window(floatingcon, event, !was_focused);
            return;
        }

        goto done;
    }

    /* 7: floating modifier pressed, initiate a resize */
    if (dest == CLICK_INSIDE && mod_pressed && event->detail == XCB_BUTTON_CLICK_RIGHT) {
        floating_mod_on_tiled_client(con, event);
        return;
    }
    /* 8: otherwise, check for border/decoration clicks and resize */
    else if ((dest == CLICK_BORDER || dest == CLICK_DECORATION) &&
             is_left_or_right_click) {
        DLOG("Trying to resize (tiling)\n");
        tiling_resize(con, event, dest, dest == CLICK_DECORATION && !was_focused);
    }

done:
    xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
    xcb_flush(conn);
    tree_render();
}

/*
 * The button press X callback. This function determines whether the floating
 * modifier is pressed and where the user clicked (decoration, border, inside
 * the window).
 *
 * Then, route_click is called on the appropriate con.
 *
 */
void handle_button_press(xcb_button_press_event_t *event) {
    Con *con;
    DLOG("Button %d (state %d) %s on window 0x%08x (child 0x%08x) at (%d, %d) (root %d, %d)\n",
         event->detail, event->state, (event->response_type == XCB_BUTTON_PRESS ? "press" : "release"),
         event->event, event->child, event->event_x, event->event_y, event->root_x,
         event->root_y);

    last_timestamp = event->time;

    const uint32_t mod = (config.floating_modifier & 0xFFFF);
    const bool mod_pressed = (mod != 0 && (event->state & mod) == mod);
    DLOG("floating_mod = %d, detail = %d\n", mod_pressed, event->detail);
    if ((con = con_by_window_id(event->event))) {
        route_click(con, event, mod_pressed, CLICK_INSIDE);
        return;
    }

    if (!(con = con_by_frame_id(event->event))) {
        /* Run bindings on the root window as well, see #2097. We only run it
         * if --whole-window was set as that's the equivalent for a normal
         * window. */
        if (event->event == root) {
            Binding *bind = get_binding_from_xcb_event((xcb_generic_event_t *)event);
            if (bind != NULL && bind->whole_window) {
                CommandResult *result = run_binding(bind, NULL);
                command_result_free(result);
            }
        }

        /* If the root window is clicked, find the relevant output from the
         * click coordinates and focus the output's active workspace. */
        if (event->event == root && event->response_type == XCB_BUTTON_PRESS) {
            Con *output, *ws;
            TAILQ_FOREACH (output, &(croot->nodes_head), nodes) {
                if (con_is_internal(output) ||
                    !rect_contains(output->rect, event->event_x, event->event_y))
                    continue;

                ws = TAILQ_FIRST(&(output_get_content(output)->focus_head));
                if (ws != con_get_workspace(focused)) {
                    workspace_show(ws);
                    tree_render();
                }
                return;
            }
            return;
        }

        ELOG("Clicked into unknown window?!\n");
        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
        xcb_flush(conn);
        return;
    }

    /* Check if the click was on the decoration of a child */
    Con *child;
    TAILQ_FOREACH_REVERSE (child, &(con->nodes_head), nodes_head, nodes) {
        if (!rect_contains(child->deco_rect, event->event_x, event->event_y))
            continue;

        route_click(child, event, mod_pressed, CLICK_DECORATION);
        return;
    }

    if (event->child != XCB_NONE) {
        DLOG("event->child not XCB_NONE, so this is an event which originated from a click into the application, but the application did not handle it.\n");
        route_click(con, event, mod_pressed, CLICK_INSIDE);
        return;
    }

    route_click(con, event, mod_pressed, CLICK_BORDER);
}
