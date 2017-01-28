#undef I3__FILE__
#define I3__FILE__ "tiling_drag.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * tiling_drag.c: Reposition tiled windows by dragging.
 *
 */
#include "all.h"

/*
 * Returns the visible leaf container at the given coordinates or NULL if no
 * such container exists.
 *
 */
static Con *find_drop_target(uint32_t x, uint32_t y) {
    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons) {
        if (con->window != NULL && rect_contains(con->rect, x, y) &&
            !con_is_floating(con) &&
            workspace_is_visible(con_get_workspace(con)) &&
            !con_is_hidden(con))
            return con;
    }
    return NULL;
}

typedef enum { DT_SIBLING,
               DT_SPLIT } drop_type_t;

struct callback_params {
    xcb_window_t indicator;
    Con **target;
    direction_t *direction;
    drop_type_t *drop_type;
};

/*
 * The callback that is executed on every mouse move while dragging. On each
 * invocation we determin the drop target and the direction in which to insert
 * the dragged container. The indicator window is updated to show the new
 * position of the dragged container. The target container and direction are
 * passed out using the callback params.
 *
 */
DRAGGING_CB(drag_callback) {
    const struct callback_params *params = extra;

    /* The threshold for the outer region. Drops in this region indicate the
     * drop should move the window into the parent as a sibling in the given
     * direction. */
    static const uint32_t outer_threshold = 30;

    Con *target = find_drop_target(new_x, new_y);
    direction_t direction = 0;
    drop_type_t drop_type = 0;

    DLOG("new x = %d, y = %d, con = %p, target = %p\n", new_x, new_y, con, target);
    if (target == NULL)
        return;

    /* If the target is the dragged container itself then we want to highlight
     * the whole container. Otherwise we determine the direction of the nearest
     * border and highlight only that half of the target container.
     *
     * TODO(MForster): Support dropping on containers with tabbed or stacked
     * layout as well as on empty outputs.
     */
    Rect rect = target->rect;
    if (target != con) {
        uint32_t d_left = new_x - rect.x;
        uint32_t d_top = new_y - rect.y;
        uint32_t d_right = rect.x + rect.width - new_x;
        uint32_t d_bottom = rect.y + rect.height - new_y;
        uint32_t d_min = min(min(d_left, d_right), min(d_top, d_bottom));

        drop_type = (d_min < outer_threshold ? DT_SIBLING : DT_SPLIT);

        if (d_left == d_min) {
            rect.width /= 2;
            direction = D_LEFT;

            if (drop_type == DT_SPLIT) {
                rect.x += outer_threshold;
                rect.width -= outer_threshold;
                rect.y += outer_threshold;
                rect.height -= outer_threshold * 2;
            } else if (drop_type == DT_SIBLING) {
                rect.width = outer_threshold;
            }
        } else if (d_top == d_min) {
            rect.height /= 2;
            direction = D_UP;

            if (drop_type == DT_SPLIT) {
                rect.x += outer_threshold;
                rect.height -= outer_threshold;
                rect.y += outer_threshold;
                rect.width -= outer_threshold * 2;
            } else if (drop_type == DT_SIBLING) {
                rect.height = outer_threshold;
            }
        } else if (d_right == d_min) {
            /* This is done in three steps to get symmetric rounding behavior
             * with the first two cases. */
            rect.x += rect.width;
            rect.width /= 2;
            rect.x -= rect.width;
            direction = D_RIGHT;

            if (drop_type == DT_SPLIT) {
                rect.width -= outer_threshold;
                rect.y += outer_threshold;
                rect.height -= outer_threshold * 2;
            } else if (drop_type == DT_SIBLING) {
                rect.x += (rect.width - outer_threshold);
                rect.width = outer_threshold;
            }
        } else if (d_bottom == d_min) {
            /* This is done in three steps to get symmetric rounding behavior
             * with the first two cases. */
            rect.y += rect.height;
            rect.height /= 2;
            rect.y -= rect.height;
            direction = D_DOWN;

            if (drop_type == DT_SPLIT) {
                rect.x += outer_threshold;
                rect.height -= outer_threshold;
                rect.width -= outer_threshold * 2;
            } else if (drop_type == DT_SIBLING) {
                rect.y += (rect.height - outer_threshold);
                rect.height = outer_threshold;
            }
        }
    }

    xcb_configure_window(conn, params->indicator,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         &(rect.x));
    xcb_flush(conn);

    *(params->target) = target;
    *(params->direction) = direction;
    *(params->drop_type) = drop_type;
}

/*
 * Returns a new drop indicator window with the given initial coordinates.
 *
 */
static xcb_window_t create_drop_indicator(Rect rect) {
    uint32_t mask = 0;
    uint32_t values[2];

    mask = XCB_CW_BACK_PIXEL;
    values[0] = config.client.focused.indicator.colorpixel;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[1] = 1;

    xcb_window_t indicator = create_window(conn, rect, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
                                           XCB_WINDOW_CLASS_INPUT_OUTPUT, XCURSOR_CURSOR_MOVE, false, mask, values);
    /* Change the window class to "i3-drag", so that it can be matched in a
     * compositor configuration. Note that the class needs to be changed before
     * mapping the window. */
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        indicator,
                        XCB_ATOM_WM_CLASS,
                        XCB_ATOM_STRING,
                        8,
                        (strlen("i3-drag") + 1) * 2,
                        "i3-drag\0i3-drag\0");
    xcb_map_window(conn, indicator);
    xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, indicator);
    xcb_flush(conn);

    return indicator;
}

/*
 * Initiates a mouse drag operation on a tiled window.
 *
 */
void tiling_drag(Con *con, xcb_button_press_event_t *event) {
    DLOG("Start dragging tiled container: con = %p\n", con);

    /* Don't change focus while dragging. */
    x_mask_event_mask(~XCB_EVENT_MASK_ENTER_WINDOW);
    xcb_flush(conn);

    /* Indicate drop location while dragging. This blocks until the drag is completed. */
    Con *target = NULL;
    direction_t direction;
    drop_type_t drop_type;
    const struct callback_params params = {create_drop_indicator(con->rect), &target, &direction, &drop_type};

    drag_result_t drag_result = drag_pointer(con, event, XCB_NONE, BORDER_TOP, XCURSOR_CURSOR_MOVE, drag_callback, &params);

    /* Dragging is done. We don't need the indicator window any more. */
    xcb_destroy_window(conn, params.indicator);
    xcb_flush(conn);

    /* Move the container to the drop position. */
    if (drag_result != DRAG_REVERT && target != NULL && target != con) {
        if (drop_type == DT_SPLIT) {
            con_move_to_side_of_con(con, target, direction);
        } else if (drop_type == DT_SIBLING) {
            Con *parent = target->parent;
            orientation_t orientation = con_orientation(parent);

            /* move out if the move goes against the grain of the parent */
            /* orientation */
            bool move_out = (orientation == HORIZ && (direction == D_UP || direction == D_DOWN)) ||
                (orientation == VERT && (direction == D_LEFT || direction == D_RIGHT));

            position_t position = (direction == D_LEFT || direction == D_UP ? BEFORE : AFTER);
            insert_con_into(con, target, position);

            /* TODO need to emit an event if we don't move out */

            if (move_out) {
                tree_move(con, direction);
            }
        }
        con_focus(con);
        tree_render();
    }
}
