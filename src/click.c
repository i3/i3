/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * click.c: Button press (mouse click) events.
 *
 */
#include "all.h"

#include <time.h>
#include <math.h>

#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>

#include <X11/XKBlib.h>

typedef enum { CLICK_BORDER = 0, CLICK_DECORATION = 1, CLICK_INSIDE = 2 } click_destination_t;

/*
 * Finds the correct pair of first/second cons between the resize will take
 * place according to the passed border position (top, left, right, bottom),
 * then calls resize_graphical_handler().
 *
 */
static bool tiling_resize_for_border(Con *con, border_t border, const xcb_button_press_event_t *event) {
    DLOG("border = %d\n", border);
    char way = (border == BORDER_TOP || border == BORDER_LEFT ? 'p' : 'n');
    orientation_t orientation = (border == BORDER_TOP || border == BORDER_BOTTOM ? VERT : HORIZ);

    /* look for a parent container with the right orientation */
    Con *first = NULL, *second = NULL;
    Con *resize_con = con;
    while (resize_con->type != CT_WORKSPACE &&
           resize_con->type != CT_FLOATING_CON &&
           resize_con->parent->orientation != orientation)
        resize_con = resize_con->parent;

    if (resize_con->type != CT_WORKSPACE &&
        resize_con->type != CT_FLOATING_CON &&
        resize_con->parent->orientation == orientation) {
        first = resize_con;
        second = (way == 'n') ? TAILQ_NEXT(first, nodes) : TAILQ_PREV(first, nodes_head, nodes);
        if (second == TAILQ_END(&(first->nodes_head))) {
            second = NULL;
        }
        else if (way == 'p') {
            Con *tmp = first;
            first = second;
            second = tmp;
        }
    }

    if (first == NULL || second == NULL) {
        DLOG("Resize not possible\n");
        return false;
    }
    else {
        assert(first != second);
        assert(first->parent == second->parent);
        resize_graphical_handler(first, second, orientation, event);
    }

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
static bool floating_mod_on_tiled_client(Con *con, const xcb_button_press_event_t *event) {
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
static bool tiling_resize(Con *con, const xcb_button_press_event_t *event, const click_destination_t dest) {
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
        if (con_is_leaf(con) && con->parent->type == CT_CON)
            con = con->parent;

        if ((con->layout == L_STACKED ||
             con->layout == L_TABBED ||
             con->orientation == HORIZ) &&
            con_num_children(con) > 1) {
            DLOG("Not handling this resize, this container has > 1 child.\n");
            return false;
        }
        return tiling_resize_for_border(con, BORDER_TOP, event);
    }

    if (event->event_x >= 0 && event->event_x <= bsr.x &&
        event->event_y >= bsr.y && event->event_y <= con->rect.height + bsr.height)
        return tiling_resize_for_border(con, BORDER_LEFT, event);

    if (event->event_x >= (con->window_rect.x + con->window_rect.width) &&
        event->event_y >= bsr.y && event->event_y <= con->rect.height + bsr.height)
        return tiling_resize_for_border(con, BORDER_RIGHT, event);

    if (event->event_y >= (con->window_rect.y + con->window_rect.height))
        return tiling_resize_for_border(con, BORDER_BOTTOM, event);

    return false;
}

/*
 * Being called by handle_button_press, this function calls the appropriate
 * functions for resizing/dragging.
 *
 */
static int route_click(Con *con, const xcb_button_press_event_t *event, const bool mod_pressed, const click_destination_t dest) {
    DLOG("--> click properties: mod = %d, destination = %d\n", mod_pressed, dest);
    DLOG("--> OUTCOME = %p\n", con);
    DLOG("type = %d, name = %s\n", con->type, con->name);

    /* don’t handle dockarea cons, they must not be focused */
    if (con->parent->type == CT_DOCKAREA)
        goto done;

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
        if (event->detail == XCB_BUTTON_INDEX_4)
            tree_next('p', orientation);
        else tree_next('n', orientation);
        goto done;
    }

    /* 2: focus this con */
    con_focus(con);

    /* 3: For floating containers, we also want to raise them on click.
     * We will skip handling events on floating cons in fullscreen mode */
    Con *ws = con_get_workspace(con);
    Con *fs = (ws ? con_get_fullscreen_con(ws, CF_OUTPUT) : NULL);
    if (floatingcon != NULL && fs == NULL) {
        floating_raise_con(floatingcon);

        /* 4: floating_modifier plus left mouse button drags */
        if (mod_pressed && event->detail == 1) {
            floating_drag_window(floatingcon, event);
            return 1;
        }

        /* 5: resize (floating) if this was a click on the left/right/bottom
         * border. also try resizing (tiling) if it was a click on the top
         * border, but continue if that does not work */
        if (mod_pressed && event->detail == 3) {
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
    if (mod_pressed && event->detail == 3) {
        if (floating_mod_on_tiled_client(con, event))
            return 1;
    }
    /* 8: otherwise, check for border/decoration clicks and resize */
    else if (dest == CLICK_BORDER || dest == CLICK_DECORATION) {
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
    DLOG("Button %d pressed on window 0x%08x\n", event->state, event->event);

    last_timestamp = event->time;

    const uint32_t mod = config.floating_modifier;
    const bool mod_pressed = (mod != 0 && (event->state & mod) == mod);
    DLOG("floating_mod = %d, detail = %d\n", mod_pressed, event->detail);
    if ((con = con_by_window_id(event->event)))
        return route_click(con, event, mod_pressed, CLICK_INSIDE);

    if (!(con = con_by_frame_id(event->event))) {
        ELOG("Clicked into unknown window?!\n");
        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
        xcb_flush(conn);
        return 0;
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
