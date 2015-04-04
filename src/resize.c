#undef I3__FILE__
#define I3__FILE__ "resize.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * resize.c: Interactive resizing.
 *
 */
#include "all.h"

extern xcb_connection_t *conn;

/*
 * This is an ugly data structure which we need because there is no standard
 * way of having nested functions (only available as a gcc extension at the
 * moment, clang doesn’t support it) or blocks (only available as a clang
 * extension and only on Mac OS X systems at the moment).
 *
 */
struct callback_params {
    orientation_t orientation;
    Con *output;
    xcb_window_t helpwin;
    uint32_t *new_position;
};

DRAGGING_CB(resize_callback) {
    const struct callback_params *params = extra;
    Con *output = params->output;
    DLOG("new x = %d, y = %d\n", new_x, new_y);
    if (params->orientation == HORIZ) {
        /* Check if the new coordinates are within screen boundaries */
        if (new_x > (output->rect.x + output->rect.width - 25) ||
            new_x < (output->rect.x + 25))
            return;

        *(params->new_position) = new_x;
        xcb_configure_window(conn, params->helpwin, XCB_CONFIG_WINDOW_X, params->new_position);
    } else {
        if (new_y > (output->rect.y + output->rect.height - 25) ||
            new_y < (output->rect.y + 25))
            return;

        *(params->new_position) = new_y;
        xcb_configure_window(conn, params->helpwin, XCB_CONFIG_WINDOW_Y, params->new_position);
    }

    xcb_flush(conn);
}

bool resize_find_tiling_participants(Con **current, Con **other, direction_t direction) {
    DLOG("Find two participants for resizing container=%p in direction=%i\n", other, direction);
    Con *first = *current;
    Con *second = NULL;
    if (first == NULL) {
        DLOG("Current container is NULL, aborting.\n");
        return false;
    }

    /* Go up in the tree and search for a container to resize */
    const orientation_t search_orientation = ((direction == D_LEFT || direction == D_RIGHT) ? HORIZ : VERT);
    const bool dir_backwards = (direction == D_UP || direction == D_LEFT);
    while (first->type != CT_WORKSPACE &&
           first->type != CT_FLOATING_CON &&
           second == NULL) {
        /* get the appropriate first container with the matching
         * orientation (skip stacked/tabbed cons) */
        if ((con_orientation(first->parent) != search_orientation) ||
            (first->parent->layout == L_STACKED) ||
            (first->parent->layout == L_TABBED)) {
            first = first->parent;
            continue;
        }

        /* get the counterpart for this resizement */
        if (dir_backwards) {
            second = TAILQ_PREV(first, nodes_head, nodes);
        } else {
            second = TAILQ_NEXT(first, nodes);
        }

        if (second == NULL) {
            DLOG("No second container in this direction found, trying to look further up in the tree...\n");
            first = first->parent;
        }
    }

    DLOG("Found participants: first=%p and second=%p.", first, second);
    *current = first;
    *other = second;
    if (first == NULL || second == NULL) {
        DLOG("Could not find two participants for this resize request.\n");
        return false;
    }

    return true;
}

int resize_graphical_handler(Con *first, Con *second, orientation_t orientation, const xcb_button_press_event_t *event) {
    DLOG("resize handler\n");

    /* TODO: previously, we were getting a rect containing all screens. why? */
    Con *output = con_get_output(first);
    DLOG("x = %d, width = %d\n", output->rect.x, output->rect.width);

    x_mask_event_mask(~XCB_EVENT_MASK_ENTER_WINDOW);
    xcb_flush(conn);

    uint32_t mask = 0;
    uint32_t values[2];

    mask = XCB_CW_OVERRIDE_REDIRECT;
    values[0] = 1;

    /* Open a new window, the resizebar. Grab the pointer and move the window around
       as the user moves the pointer. */
    xcb_window_t grabwin = create_window(conn, output->rect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                                         XCB_WINDOW_CLASS_INPUT_ONLY, XCURSOR_CURSOR_POINTER, true, mask, values);

    /* Keep track of the coordinate orthogonal to motion so we can determine
     * the length of the resize afterward. */
    uint32_t initial_position, new_position;

    /* Configure the resizebar and snap the pointer. The resizebar runs along
     * the rect of the second con and follows the motion of the pointer. */
    Rect helprect;
    if (orientation == HORIZ) {
        helprect.x = second->rect.x;
        helprect.y = second->rect.y;
        helprect.width = logical_px(2);
        helprect.height = second->rect.height;
        initial_position = second->rect.x;
        xcb_warp_pointer(conn, XCB_NONE, event->root, 0, 0, 0, 0,
                         second->rect.x, event->root_y);
    } else {
        helprect.x = second->rect.x;
        helprect.y = second->rect.y;
        helprect.width = second->rect.width;
        helprect.height = logical_px(2);
        initial_position = second->rect.y;
        xcb_warp_pointer(conn, XCB_NONE, event->root, 0, 0, 0, 0,
                         event->root_x, second->rect.y);
    }

    mask = XCB_CW_BACK_PIXEL;
    values[0] = config.client.focused.border;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[1] = 1;

    xcb_window_t helpwin = create_window(conn, helprect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                                         XCB_WINDOW_CLASS_INPUT_OUTPUT, (orientation == HORIZ ? XCURSOR_CURSOR_RESIZE_HORIZONTAL : XCURSOR_CURSOR_RESIZE_VERTICAL), true, mask, values);

    xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

    xcb_flush(conn);

    /* `new_position' will be updated by the `resize_callback'. */
    new_position = initial_position;

    const struct callback_params params = {orientation, output, helpwin, &new_position};

    /* `drag_pointer' blocks until the drag is completed. */
    drag_result_t drag_result = drag_pointer(NULL, event, grabwin, BORDER_TOP, 0, resize_callback, &params);

    xcb_destroy_window(conn, helpwin);
    xcb_destroy_window(conn, grabwin);
    xcb_flush(conn);

    /* User cancelled the drag so no action should be taken. */
    if (drag_result == DRAG_REVERT)
        return 0;

    int pixels = (new_position - initial_position);

    DLOG("Done, pixels = %d\n", pixels);

    // if we got thus far, the containers must have
    // percentages associated with them
    assert(first->percent > 0.0);
    assert(second->percent > 0.0);

    // calculate the new percentage for the first container
    double new_percent, difference;
    double percent = first->percent;
    DLOG("percent = %f\n", percent);
    int original = (orientation == HORIZ ? first->rect.width : first->rect.height);
    DLOG("original = %d\n", original);
    new_percent = (original + pixels) * (percent / original);
    difference = percent - new_percent;
    DLOG("difference = %f\n", difference);
    DLOG("new percent = %f\n", new_percent);
    first->percent = new_percent;

    // calculate the new percentage for the second container
    double s_percent = second->percent;
    second->percent = s_percent + difference;
    DLOG("second->percent = %f\n", second->percent);

    // now we must make sure that the sum of the percentages remain 1.0
    con_fix_percent(first->parent);

    return 0;
}
