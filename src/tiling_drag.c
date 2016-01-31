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

struct callback_params {
    xcb_window_t indicator;
    Con **target;
    direction_t *direction;
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

    Con *target = find_drop_target(new_x, new_y);
    direction_t direction = 0;

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

        if (d_left == d_min) {
            rect.width /= 2;
            direction = D_LEFT;
        } else if (d_top == d_min) {
            rect.height /= 2;
            direction = D_UP;
        } else if (d_right == d_min) {
            /* This is done in three steps to get symmetric rounding behavior
             * with the first two cases. */
            rect.x += rect.width;
            rect.width /= 2;
            rect.x -= rect.width;
            direction = D_RIGHT;
        } else if (d_bottom == d_min) {
            /* This is done in three steps to get symmetric rounding behavior
             * with the first two cases. */
            rect.y += rect.height;
            rect.height /= 2;
            rect.y -= rect.height;
            direction = D_DOWN;
        }
    }

    xcb_configure_window(conn, params->indicator,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         &(rect.x));
    xcb_flush(conn);

    *(params->target) = target;
    *(params->direction) = direction;
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
                                           XCB_WINDOW_CLASS_INPUT_OUTPUT, XCURSOR_CURSOR_MOVE, true, mask, values);
    xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, indicator);
    xcb_flush(conn);

    return indicator;
}

/*
 * Moves the dropped container to its new position. This makes the the container
 * and its target the only two children of a split container with appropriate
 * layout. If the parent container of the target has correct layout and no
 * additional children, then no new split container is created.
 *
 */
static void drop_con(Con *con, Con *target, direction_t direction) {
    DLOG("Dropping container: con = %p, target = %p, direction = %d\n", con, target, direction);

    /* Split the target's parent if we need to. */
    orientation_t orientation = (direction == D_LEFT || direction == D_RIGHT) ? HORIZ : VERT;
    if (con_orientation(target->parent) != orientation || con_num_children(target->parent) > 1) {
        tree_split(target, orientation);
    }

    /* Detach the container from its old parent. */
    Con *old_parent = con->parent;
    Con *parent = target->parent;

    con_detach(con);
    con_fix_percent(con->parent);

    /** Attach the container to the target's parent. */
    con->parent = parent;
    con->percent = target->percent = .5;

    if (direction == D_LEFT || direction == D_UP) {
        TAILQ_INSERT_BEFORE(target, con, nodes);
        TAILQ_INSERT_HEAD(&(parent->focus_head), con, focused);
    } else {
        TAILQ_INSERT_AFTER(&(parent->nodes_head), target, con, nodes);
        TAILQ_INSERT_HEAD(&(parent->focus_head), con, focused);
    }

    /* Clean up. */
    CALL(old_parent, on_remove_child);
    tree_flatten(croot);
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
    const struct callback_params params = {create_drop_indicator(con->rect), &target, &direction};

    drag_result_t drag_result = drag_pointer(con, event, XCB_NONE, BORDER_TOP, XCURSOR_CURSOR_MOVE, drag_callback, &params);

    /* Dragging is done. We don't need the indicator window any more. */
    xcb_destroy_window(conn, params.indicator);
    xcb_flush(conn);

    /* Move the container to the drop position. */
    if (drag_result != DRAG_REVERT && target != NULL && target != con) {
        drop_con(con, target, direction);
        tree_render();
    }
}
