/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * resize.c: Interactive resizing.
 *
 */
#include "all.h"

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
    bool *threshold_exceeded;
};

DRAGGING_CB(resize_callback) {
    const struct callback_params *params = extra;
    Con *output = params->output;
    DLOG("new x = %d, y = %d\n", new_x, new_y);

    if (!*params->threshold_exceeded) {
        xcb_map_window(conn, params->helpwin);
        /* Warp pointer in the same way as resize_graphical_handler() would do
         * if threshold wasn't enabled, but also take into account travelled
         * distance. */
        if (params->orientation == HORIZ) {
            xcb_warp_pointer(conn, XCB_NONE, event->root, 0, 0, 0, 0,
                             *params->new_position + new_x - event->root_x,
                             new_y);
        } else {
            xcb_warp_pointer(conn, XCB_NONE, event->root, 0, 0, 0, 0,
                             new_x,
                             *params->new_position + new_y - event->root_y);
        }
        *params->threshold_exceeded = true;
        return;
    }

    if (params->orientation == HORIZ) {
        /* Check if the new coordinates are within screen boundaries */
        if (new_x > (output->rect.x + output->rect.width - 25) ||
            new_x < (output->rect.x + 25)) {
            return;
        }

        *(params->new_position) = new_x;
        xcb_configure_window(conn, params->helpwin, XCB_CONFIG_WINDOW_X, params->new_position);
    } else {
        if (new_y > (output->rect.y + output->rect.height - 25) ||
            new_y < (output->rect.y + 25)) {
            return;
        }

        *(params->new_position) = new_y;
        xcb_configure_window(conn, params->helpwin, XCB_CONFIG_WINDOW_Y, params->new_position);
    }

    xcb_flush(conn);
}

bool resize_find_tiling_participants(Con **current, Con **other, direction_t direction, bool both_sides) {
    DLOG("Find two participants for resizing container=%p in direction=%i\n", other, direction);
    Con *first = *current;
    Con *second = NULL;
    if (first == NULL) {
        DLOG("Current container is NULL, aborting.\n");
        return false;
    }

    /* Go up in the tree and search for a container to resize */
    const orientation_t search_orientation = orientation_from_direction(direction);
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
            if (second == NULL && both_sides == true) {
                second = TAILQ_NEXT(first, nodes);
            }
        } else {
            second = TAILQ_NEXT(first, nodes);
            if (second == NULL && both_sides == true) {
                second = TAILQ_PREV(first, nodes_head, nodes);
            }
        }

        if (second == NULL) {
            DLOG("No second container in this direction found, trying to look further up in the tree...\n");
            first = first->parent;
        }
    }

    DLOG("Found participants: first=%p and second=%p.\n", first, second);
    *current = first;
    *other = second;
    if (first == NULL || second == NULL) {
        DLOG("Could not find two participants for this resize request.\n");
        return false;
    }

    return true;
}

/*
 * Calculate the minimum percent needed for the given container to be at least 1
 * pixel.
 *
 */
double percent_for_1px(Con *con) {
    const int parent_size = con_rect_size_in_orientation(con->parent);
    /* deco_rect.height is subtracted from each child in render_con_split */
    const int min_size = (con_orientation(con->parent) == HORIZ ? 1 : 1 + con->deco_rect.height);
    return ((double)min_size / (double)parent_size);
}

/*
 * Resize the two given containers using the given amount of pixels or
 * percentage points. One of the two needs to be 0. A positive amount means
 * growing the first container while a negative means shrinking it.
 * Returns false when the resize would result in one of the two containers
 * having less than 1 pixel of size.
 *
 */
bool resize_neighboring_cons(Con *first, Con *second, int px, int ppt) {
    assert(px * ppt == 0);

    Con *parent = first->parent;
    double new_first_percent;
    double new_second_percent;
    if (ppt) {
        new_first_percent = first->percent + ((double)ppt / 100.0);
        new_second_percent = second->percent - ((double)ppt / 100.0);
    } else {
        /* Convert px change to change in percentages */
        const double pct = (double)px / (double)con_rect_size_in_orientation(first->parent);
        new_first_percent = first->percent + pct;
        new_second_percent = second->percent - pct;
    }
    /* Ensure that no container will be less than 1 pixel in the resizing
     * direction. */
    if (new_first_percent < percent_for_1px(first) || new_second_percent < percent_for_1px(second)) {
        return false;
    }

    first->percent = new_first_percent;
    second->percent = new_second_percent;
    con_fix_percent(parent);
    return true;
}

void resize_graphical_handler(Con *first, Con *second, orientation_t orientation,
                              const xcb_button_press_event_t *event,
                              bool use_threshold) {
    Con *output = con_get_output(first);
    DLOG("x = %d, width = %d\n", output->rect.x, output->rect.width);
    DLOG("first = %p / %s\n", first, first->name);
    DLOG("second = %p / %s\n", second, second->name);

    x_mask_event_mask(~XCB_EVENT_MASK_ENTER_WINDOW);
    xcb_flush(conn);

    uint32_t mask = 0;
    uint32_t values[2];

    mask = XCB_CW_OVERRIDE_REDIRECT;
    values[0] = 1;

    /* Open a new window, the resizebar. Grab the pointer and move the window
     * around as the user moves the pointer. */
    xcb_window_t grabwin = create_window(conn, output->rect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                                         XCB_WINDOW_CLASS_INPUT_ONLY, XCURSOR_CURSOR_POINTER, true, mask, values);

    /* Keep track of the coordinate orthogonal to motion so we can determine the
     * length of the resize afterward. */
    uint32_t initial_position, new_position;

    /* Configure the resizebar and snap the pointer. The resizebar runs along
     * the rect of the second con and follows the motion of the pointer. */
    Rect helprect;
    helprect.x = second->rect.x;
    helprect.y = second->rect.y;
    /* Resizes might happen between a split container and a leaf
     * container. Because gaps happen *within* a split container, we need to
     * work with (any) leaf window inside the split, so descend focused. */
    Con *ffirst = con_descend_focused(first);
    Con *fsecond = con_descend_focused(second);
    if (orientation == HORIZ) {
        helprect.width = logical_px(2);
        helprect.height = second->rect.height;
        const uint32_t ffirst_right = ffirst->rect.x + ffirst->rect.width;
        const uint32_t gap = (fsecond->rect.x - ffirst_right);
        const uint32_t middle = fsecond->rect.x - (gap / 2);
        DLOG("ffirst->rect = {.x = %u, .width = %u}\n", ffirst->rect.x, ffirst->rect.width);
        DLOG("fsecond->rect = {.x = %u, .width = %u}\n", fsecond->rect.x, fsecond->rect.width);
        DLOG("gap = %u, middle = %u\n", gap, middle);
        initial_position = middle;
    } else {
        helprect.width = second->rect.width;
        helprect.height = logical_px(2);
        const uint32_t ffirst_bottom = ffirst->rect.y + ffirst->rect.height;
        const uint32_t gap = (fsecond->rect.y - ffirst_bottom);
        const uint32_t middle = fsecond->rect.y - (gap / 2);
        DLOG("ffirst->rect = {.y = %u, .height = %u}\n", ffirst->rect.y, ffirst->rect.height);
        DLOG("fsecond->rect = {.y = %u, .height = %u}\n", fsecond->rect.y, fsecond->rect.height);
        DLOG("gap = %u, middle = %u\n", gap, middle);
        initial_position = middle;
    }

    mask = XCB_CW_BACK_PIXEL;
    values[0] = config.client.focused.border.colorpixel;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[1] = 1;

    xcb_window_t helpwin = create_window(conn, helprect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                                         XCB_WINDOW_CLASS_INPUT_OUTPUT, (orientation == HORIZ ? XCURSOR_CURSOR_RESIZE_HORIZONTAL : XCURSOR_CURSOR_RESIZE_VERTICAL), false, mask, values);

    if (!use_threshold) {
        xcb_map_window(conn, helpwin);
        if (orientation == HORIZ) {
            xcb_warp_pointer(conn, XCB_NONE, event->root, 0, 0, 0, 0,
                             initial_position, event->root_y);
        } else {
            xcb_warp_pointer(conn, XCB_NONE, event->root, 0, 0, 0, 0,
                             event->root_x, initial_position);
        }
    }

    xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

    xcb_flush(conn);

    /* `new_position' will be updated by the `resize_callback'. */
    new_position = initial_position;

    bool threshold_exceeded = !use_threshold;

    const struct callback_params params = {orientation, output, helpwin, &new_position, &threshold_exceeded};

    /* Re-render the tree before returning to the event loop (drag_pointer()
     * runs its own event-loop) in case if there are unrendered updates. */
    tree_render();

    /* `drag_pointer' blocks until the drag is completed. */
    drag_result_t drag_result = drag_pointer(NULL, event, grabwin, 0, use_threshold, resize_callback, &params);

    xcb_destroy_window(conn, helpwin);
    xcb_destroy_window(conn, grabwin);
    xcb_flush(conn);

    /* User cancelled the drag so no action should be taken. */
    if (drag_result == DRAG_REVERT) {
        return;
    }

    int pixels = (new_position - initial_position);
    DLOG("Done, pixels = %d\n", pixels);

    /* No change; no action needed. */
    if (pixels == 0) {
        return;
    }

    /* if we got thus far, the containers must have valid percentages. */
    assert(first->percent > 0.0);
    assert(second->percent > 0.0);
    const bool result = resize_neighboring_cons(first, second, pixels, 0);
    DLOG("Graphical resize %s: first->percent = %f, second->percent = %f.\n",
         result ? "successful" : "failed", first->percent, second->percent);
}
