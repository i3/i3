#undef I3__FILE__
#define I3__FILE__ "move.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * move.c: Moving containers into some direction.
 *
 */
#include "all.h"

typedef enum { BEFORE,
               AFTER } position_t;

/*
 * This function detaches 'con' from its parent and inserts it either before or
 * after 'target'.
 *
 */
static void insert_con_into(Con *con, Con *target, position_t position) {
    Con *parent = target->parent;
    /* We need to preserve the old con->parent. While it might still be used to
     * insert the entry before/after it, we call the on_remove_child callback
     * afterwards which might then close the con if it is empty. */
    Con *old_parent = con->parent;

    con_detach(con);
    con_fix_percent(con->parent);

    /* When moving to a workspace, we respect the user’s configured
     * workspace_layout */
    if (parent->type == CT_WORKSPACE) {
        Con *split = workspace_attach_to(parent);
        if (split != parent) {
            DLOG("Got a new split con, using that one instead\n");
            con->parent = split;
            con_attach(con, split, false);
            DLOG("attached\n");
            con->percent = 0.0;
            con_fix_percent(split);
            con = split;
            DLOG("ok, continuing with con %p instead\n", con);
            con_detach(con);
        }
    }

    con->parent = parent;

    if (position == BEFORE) {
        TAILQ_INSERT_BEFORE(target, con, nodes);
        TAILQ_INSERT_HEAD(&(parent->focus_head), con, focused);
    } else if (position == AFTER) {
        TAILQ_INSERT_AFTER(&(parent->nodes_head), target, con, nodes);
        TAILQ_INSERT_HEAD(&(parent->focus_head), con, focused);
    }

    /* Pretend the con was just opened with regards to size percent values.
     * Since the con is moved to a completely different con, the old value
     * does not make sense anyways. */
    con->percent = 0.0;
    con_fix_percent(parent);

    CALL(old_parent, on_remove_child);
}

/*
 * This function detaches 'con' from its parent and puts it in the given
 * workspace. Position is determined by the direction of movement into the
 * workspace container.
 *
 */
static void attach_to_workspace(Con *con, Con *ws, direction_t direction) {
    con_detach(con);
    con_fix_percent(con->parent);

    CALL(con->parent, on_remove_child);

    con->parent = ws;

    if (direction == D_RIGHT || direction == D_DOWN) {
        TAILQ_INSERT_HEAD(&(ws->nodes_head), con, nodes);
        TAILQ_INSERT_HEAD(&(ws->focus_head), con, focused);
    } else {
        TAILQ_INSERT_TAIL(&(ws->nodes_head), con, nodes);
        TAILQ_INSERT_TAIL(&(ws->focus_head), con, focused);
    }

    /* Pretend the con was just opened with regards to size percent values.
     * Since the con is moved to a completely different con, the old value
     * does not make sense anyways. */
    con->percent = 0.0;
    con_fix_percent(ws);
}

/*
 * Moves the given container to the closest output in the given direction if
 * such an output exists.
 *
 */
static void move_to_output_directed(Con *con, direction_t direction) {
    Con *old_ws = con_get_workspace(con);
    Con *current_output_con = con_get_output(con);
    Output *current_output = get_output_by_name(current_output_con->name);
    Output *output = get_output_next(direction, current_output, CLOSEST_OUTPUT);

    if (!output) {
        DLOG("No output in this direction found. Not moving.\n");
        return;
    }

    Con *ws = NULL;
    GREP_FIRST(ws, output_get_content(output->con), workspace_is_visible(child));

    if (!ws) {
        DLOG("No workspace on output in this direction found. Not moving.\n");
        return;
    }

    attach_to_workspace(con, ws, direction);

    /* fix the focus stack */
    con_focus(con);

    /* force re-painting the indicators */
    FREE(con->deco_render_params);

    tree_flatten(croot);

    ipc_send_workspace_event("focus", ws, old_ws);
}

/*
 * Moves the given container in the given direction (D_LEFT, D_RIGHT,
 * D_UP, D_DOWN).
 *
 */
void tree_move(Con *con, int direction) {
    position_t position;
    Con *target;

    DLOG("Moving in direction %d\n", direction);

    /* 1: get the first parent with the same orientation */

    if (con->type == CT_WORKSPACE) {
        DLOG("Not moving workspace\n");
        return;
    }

    if (con->parent->type == CT_WORKSPACE && con_num_children(con->parent) == 1) {
        /* This is the only con on this workspace */
        move_to_output_directed(con, direction);
        return;
    }

    orientation_t o = (direction == D_LEFT || direction == D_RIGHT ? HORIZ : VERT);

    Con *same_orientation = con_parent_with_orientation(con, o);
    /* The do {} while is used to 'restart' at this point with a different
     * same_orientation, see the very last lines before the end of this block
     * */
    do {
        /* There is no parent container with the same orientation */
        if (!same_orientation) {
            if (con_is_floating(con)) {
                /* this is a floating con, we just disable floating */
                floating_disable(con, true);
                return;
            }
            if (con_inside_floating(con)) {
                /* 'con' should be moved out of a floating container */
                DLOG("Inside floating, moving to workspace\n");
                attach_to_workspace(con, con_get_workspace(con), direction);
                goto end;
            }
            DLOG("Force-changing orientation\n");
            ws_force_orientation(con_get_workspace(con), o);
            same_orientation = con_parent_with_orientation(con, o);
        }

        /* easy case: the move is within this container */
        if (same_orientation == con->parent) {
            DLOG("We are in the same container\n");
            Con *swap;
            if ((swap = (direction == D_LEFT || direction == D_UP ? TAILQ_PREV(con, nodes_head, nodes) : TAILQ_NEXT(con, nodes)))) {
                if (!con_is_leaf(swap)) {
                    DLOG("Moving into our bordering branch\n");
                    target = con_descend_direction(swap, direction);
                    position = (con_orientation(target->parent) != o ||
                                        direction == D_UP ||
                                        direction == D_LEFT
                                    ? AFTER
                                    : BEFORE);
                    insert_con_into(con, target, position);
                    goto end;
                }
                if (direction == D_LEFT || direction == D_UP)
                    TAILQ_SWAP(swap, con, &(swap->parent->nodes_head), nodes);
                else
                    TAILQ_SWAP(con, swap, &(swap->parent->nodes_head), nodes);

                TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
                TAILQ_INSERT_HEAD(&(swap->parent->focus_head), con, focused);

                DLOG("Swapped.\n");
                ipc_send_window_event("move", con);
                return;
            }

            if (con->parent == con_get_workspace(con)) {
                /*  If we couldn't find a place to move it on this workspace,
                 *  try to move it to a workspace on a different output */
                move_to_output_directed(con, direction);
                ipc_send_window_event("move", con);
                return;
            }

            /* If there was no con with which we could swap the current one,
             * search again, but starting one level higher. */
            same_orientation = con_parent_with_orientation(con->parent, o);
        }
    } while (same_orientation == NULL);

    /* this time, we have to move to another container */
    /* This is the container *above* 'con' (an ancestor of con) which is inside
     * 'same_orientation' */
    Con *above = con;
    while (above->parent != same_orientation)
        above = above->parent;

    /* Enforce the fullscreen focus restrictions. */
    if (!con_fullscreen_permits_focusing(above->parent)) {
        LOG("Cannot move out of fullscreen container\n");
        return;
    }

    DLOG("above = %p\n", above);

    Con *next = (direction == D_UP || direction == D_LEFT ? TAILQ_PREV(above, nodes_head, nodes) : TAILQ_NEXT(above, nodes));

    if (next && !con_is_leaf(next)) {
        DLOG("Moving into the bordering branch of our adjacent container\n");
        target = con_descend_direction(next, direction);
        position = (con_orientation(target->parent) != o ||
                            direction == D_UP ||
                            direction == D_LEFT
                        ? AFTER
                        : BEFORE);
        insert_con_into(con, target, position);
    } else if (con->parent->parent->type == CT_WORKSPACE &&
               con->parent->layout != L_DEFAULT &&
               con_num_children(con->parent) == 1) {
        /* Con is the lone child of a non-default layout container at the edge
         * of the workspace. Treat it as though the workspace is its parent
         * and move it to the next output. */
        DLOG("Grandparent is workspace\n");
        move_to_output_directed(con, direction);
    } else {
        DLOG("Moving into container above\n");
        position = (direction == D_UP || direction == D_LEFT ? BEFORE : AFTER);
        insert_con_into(con, above, position);
    }

end:
    /* We need to call con_focus() to fix the focus stack "above" the container
     * we just inserted the focused container into (otherwise, the parent
     * container(s) would still point to the old container(s)). */
    con_focus(con);

    /* force re-painting the indicators */
    FREE(con->deco_render_params);

    tree_flatten(croot);
    ipc_send_window_event("move", con);
}
