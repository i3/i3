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

struct callback_params_dim {
    /* Pair of containers to be resized by this axis or NULL. */
    Con *first, *second;
    /* Corresponding axis of mouse coordinates.
     * Used to track and to revert changes. */
    uint32_t prev;
    double orig;
};

struct callback_params {
    struct callback_params_dim horisontal;
    struct callback_params_dim vertical;
};

static void reset_percent(const struct callback_params_dim *d) {
    double sum = d->first->percent + d->second->percent;
    d->first->percent = sum * d->orig;
    d->second->percent = sum * (1 - d->orig);
    con_fix_percent(d->first->parent);
}

DRAGGING_CB(resize_callback) {
    const struct callback_params *params = extra;
    bool updated = false;
    DLOG("new x = %d, y = %d\n", new_x, new_y);

    if (params->horisontal.first != NULL) {
        reset_percent(&params->horisontal);
        updated |= resize_neighboring_cons(
            params->horisontal.first,
            params->horisontal.second,
            new_x - params->horisontal.prev, 0);
    }
    if (params->vertical.first != NULL) {
        reset_percent(&params->vertical);
        updated |= resize_neighboring_cons(
            params->vertical.first,
            params->vertical.second,
            new_y - params->vertical.prev, 0);
    }

    if (updated) {
        tree_render();
    }
}

bool resize_find_tiling_participants(Con **current, Con **other, direction_t direction, bool both_sides) {
    DLOG("Find two participants for resizing container=%p in direction=%i\n", other, direction);
    Con *first = *current;
    Con *second = NULL;
    if (first == NULL) {
        DLOG("Current container is NULL, aborting.\n");
        *current = *other = NULL;
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
        *current = *other = NULL;
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
 * Return false when resize is not performed due to container minimum size
 * constraints.
 *
 */
bool resize_neighboring_cons(Con *first, Con *second, int px, int ppt) {
    assert(px * ppt == 0);

    Con *parent = first->parent;

    const double min_first =
        parent->layout == L_SPLITH
            ? (double)first->min_size.w / parent->rect.width
            : (double)first->min_size.h / parent->rect.height;
    const double min_second =
        parent->layout == L_SPLITH
            ? (double)second->min_size.w / parent->rect.width
            : (double)second->min_size.h / parent->rect.height;

    /* Refuse to shrink a con if it's already below its minimum size. */
    if ((first->percent < min_first && (px + ppt) < 0) ||
        (second->percent < min_second && (px + ppt) > 0)) {
        return false;
    }

    /* Convert to change in percentages. */
    const double pct =
        ppt ? (double)ppt / 100.0
            : (double)px / (double)con_rect_size_in_orientation(parent);

    first->percent += pct;
    second->percent -= pct;

    /* If we can't resize while keeping minimum size, do the best what we can. */
    if (first->percent < min_first && pct < 0) {
        second->percent -= min_first - first->percent;
        first->percent = min_first;
    }
    if (second->percent < min_second && pct > 0) {
        first->percent -= min_second - second->percent;
        second->percent = min_second;
    }

    con_fix_percent(parent);
    return true;
}

void resize_graphical_handler(const xcb_button_press_event_t *event,
                              enum xcursor_cursor_t cursor,
                              bool use_threshold,
                              Con *output,
                              Con *first_h, Con *first_v, Con *second_h, Con *second_v) {
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

    xcb_flush(conn);

    const struct callback_params params = {
        .horisontal = {
            first_h,
            second_h,
            event->root_x,
            first_h ? first_h->percent / (first_h->percent + second_h->percent) : 0,
        },
        .vertical = {
            first_v,
            second_v,
            event->root_y,
            first_v ? first_v->percent / (first_v->percent + second_v->percent) : 0,
        },
    };

    /* Re-render the tree before returning to the event loop (drag_pointer()
     * runs its own event-loop) in case if there are unrendered updates. */
    tree_render();

    /* `drag_pointer' blocks until the drag is completed. */
    drag_result_t drag_result = drag_pointer(NULL, event, grabwin, cursor, use_threshold, resize_callback, &params);

    xcb_destroy_window(conn, grabwin);
    xcb_flush(conn);

    /* Undo resize if user cancelled the drag. */
    if (drag_result == DRAG_REVERT) {
        if (first_h != NULL) {
            reset_percent(&params.horisontal);
        }
        if (first_v != NULL) {
            reset_percent(&params.vertical);
        }
        tree_render();
    }
}

enum xcursor_cursor_t resize_cursor(border_t corner, bool both) {
    if (both) {
        switch ((int)corner) {
            case BORDER_TOP:
            case BORDER_BOTTOM:
                return XCURSOR_CURSOR_RESIZE_VERTICAL;
            case BORDER_LEFT:
            case BORDER_RIGHT:
                return XCURSOR_CURSOR_RESIZE_HORIZONTAL;
            case BORDER_TOP | BORDER_LEFT:
            case BORDER_BOTTOM | BORDER_RIGHT:
            case BORDER_TOP | BORDER_RIGHT:
            case BORDER_BOTTOM | BORDER_LEFT:
                return XCURSOR_CURSOR_MOVE;
            default:
                /* Should never happen */
                return 0;
        }
    } else {
        switch ((int)corner) {
            case BORDER_TOP:
                return XCURSOR_CURSOR_TOP_SIDE;
            case BORDER_BOTTOM:
                return XCURSOR_CURSOR_BOTTOM_SIDE;
            case BORDER_LEFT:
                return XCURSOR_CURSOR_LEFT_SIDE;
            case BORDER_RIGHT:
                return XCURSOR_CURSOR_RIGHT_SIDE;
            case BORDER_TOP | BORDER_LEFT:
                return XCURSOR_CURSOR_TOP_LEFT_CORNER;
            case BORDER_TOP | BORDER_RIGHT:
                return XCURSOR_CURSOR_TOP_RIGHT_CORNER;
            case BORDER_BOTTOM | BORDER_LEFT:
                return XCURSOR_CURSOR_BOTTOM_LEFT_CORNER;
            case BORDER_BOTTOM | BORDER_RIGHT:
                return XCURSOR_CURSOR_BOTTOM_RIGHT_CORNER;
            default:
                /* Should never happen */
                return 0;
        }
    }
}

/*
 * Return a bitmask of a corresponding resize borders for title/border drag.
 *
 */
border_t resize_get_borders_sides(Con *con, int x, int y, click_destination_t dest) {
    const int corner_size = logical_px(24);
    const Rect bsr = con_border_style_rect(con);

    /* Make coordinates relative to con->rect */
    x -= con->rect.x;
    y -= con->rect.y;

    /* How deep inside each corner the click was */
    const int x0 = corner_size - x + bsr.x;
    const int x1 = corner_size + x - bsr.x - con->rect.width + 1 - bsr.width;
    const int y0 = corner_size - y + bsr.y;
    const int y1 = corner_size + y - bsr.y - con->rect.height + 1 - bsr.height;

    border_t border = 0;

    if (x0 > 0 && x1 > 0) {
        border |= (x0 > x1 ? BORDER_LEFT : BORDER_RIGHT);
    } else if (x0 > 0) {
        border |= BORDER_LEFT;
    } else if (x1 > 0) {
        border |= BORDER_RIGHT;
    }

    if (dest == CLICK_DECORATION) {
        border |= BORDER_TOP;
    } else if (y0 > 0 && y1 > 0) {
        border |= (y0 > y1 ? BORDER_TOP : BORDER_BOTTOM);
    } else if (y0 > 0) {
        border |= BORDER_TOP;
    } else if (y1 > 0) {
        border |= BORDER_BOTTOM;
    }

    return border;
}

/*
 * Return a bitmask of a corresponding resize borders for mod+rightclick resize.
 *
 */
border_t resize_get_borders_mod(int x, int y, int width, int height) {
    const int xb = MIN(2, MAX(0, 3 * x / width));
    const int yb = MIN(2, MAX(0, 3 * y / height));
    if (xb == 1 && yb == 1) {
        const bool d1 = x * height > y * width;
        const bool d2 = x * height + (y - height) * width > 0;
        static const border_t res[4] = {BORDER_LEFT, BORDER_BOTTOM, BORDER_TOP, BORDER_RIGHT};
        return res[d1 * 2 + d2];
    } else {
        return (xb == 0 ? BORDER_LEFT : xb == 2 ? BORDER_RIGHT : 0) |
               (yb == 0 ? BORDER_TOP : yb == 2 ? BORDER_BOTTOM : 0);
    }
}
