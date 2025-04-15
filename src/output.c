/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * output.c: Output (monitor) related functions.
 *
 */
#include "all.h"

/*
 * Returns the content container below the given output container.
 *
 */
Con *output_get_content(Con *output) {
    Con *child;

    TAILQ_FOREACH (child, &(output->nodes_head), nodes) {
        if (child->type == CT_CON) {
            return child;
        }
    }

    return NULL;
}

/*
 * Returns an 'output' corresponding to one of left/right/down/up or a specific
 * output name.
 *
 */
Output *get_output_from_string(Output *current_output, const char *output_str) {
    if (strcasecmp(output_str, "current") == 0) {
        return get_output_for_con(focused);
    } else if (strcasecmp(output_str, "left") == 0) {
        return get_output_next_wrap(D_LEFT, current_output);
    } else if (strcasecmp(output_str, "right") == 0) {
        return get_output_next_wrap(D_RIGHT, current_output);
    } else if (strcasecmp(output_str, "up") == 0) {
        return get_output_next_wrap(D_UP, current_output);
    } else if (strcasecmp(output_str, "down") == 0) {
        return get_output_next_wrap(D_DOWN, current_output);
    }

    return get_output_by_name(output_str, true);
}

/*
 * Retrieves the primary name of an output.
 *
 */
char *output_primary_name(Output *output) {
    return SLIST_FIRST(&output->names_head)->name;
}

/*
 * Retrieves the output for a given container. Never returns NULL.
 * There is an assertion that _will_ fail if the container is inside an
 * internal workspace. Use con_is_internal() if needed before calling this
 * function.
 */
Output *get_output_for_con(Con *con) {
    Con *output_con = con_get_output(con);
    Output *output = get_output_by_name(output_con->name, true);
    assert(output != NULL);

    return output;
}

/*
 * Iterates over all outputs and pushes sticky windows to the currently visible
 * workspace on that output.
 *
 * old_focus is used to determine if a sticky window is going to be focused.
 * old_focus might be different than the currently focused container because the
 * caller might need to temporarily change the focus and then call
 * output_push_sticky_windows. For example, workspace_show needs to set focus to
 * one of its descendants first, then call output_push_sticky_windows that
 * should focus a sticky window if it was the focused in the previous workspace.
 *
 */
void output_push_sticky_windows(Con *old_focus) {
    Con *output;
    TAILQ_FOREACH (output, &(croot->focus_head), focused) {
        Con *workspace, *visible_ws = NULL;
        GREP_FIRST(visible_ws, output_get_content(output), workspace_is_visible(child));

        /* We use this loop instead of TAILQ_FOREACH to avoid problems if the
         * sticky window was the last window on that workspace as moving it in
         * this case will close the workspace. */
        for (workspace = TAILQ_FIRST(&(output_get_content(output)->focus_head));
             workspace != TAILQ_END(&(output_get_content(output)->focus_head));) {
            Con *current_ws = workspace;
            workspace = TAILQ_NEXT(workspace, focused);

            /* Since moving the windows actually removes them from the list of
             * floating windows on this workspace, here too we need to use
             * another loop than TAILQ_FOREACH. */
            Con *child;
            for (child = TAILQ_FIRST(&(current_ws->focus_head));
                 child != TAILQ_END(&(current_ws->focus_head));) {
                Con *current = child;
                child = TAILQ_NEXT(child, focused);
                if (current->type != CT_FLOATING_CON || !con_is_sticky(current)) {
                    continue;
                }

                bool ignore_focus = (old_focus == NULL) || (current != old_focus->parent);
                con_move_to_workspace(current, visible_ws, true, false, ignore_focus);
                if (!ignore_focus) {
                    Con *current_ws = con_get_workspace(focused);
                    con_activate(con_descend_focused(current));
                    /* Pushing sticky windows shouldn't change the focused workspace. */
                    con_activate(con_descend_focused(current_ws));
                }
            }
        }
    }
}
