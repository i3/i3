/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * output.c: Output (monitor) related functions.
 *
 */
#include "all.h"

/*
 * Returns the output container below the given output container.
 *
 */
Con *output_get_content(Con *output) {
    Con *child;

    TAILQ_FOREACH(child, &(output->nodes_head), nodes)
    if (child->type == CT_CON)
        return child;

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

Output *get_output_for_con(Con *con) {
    Con *output_con = con_get_output(con);
    if (output_con == NULL) {
        ELOG("Could not get the output container for con = %p.\n", con);
        return NULL;
    }

    Output *output = get_output_by_name(output_con->name, true);
    if (output == NULL) {
        ELOG("Could not get output from name \"%s\".\n", output_con->name);
        return NULL;
    }

    return output;
}

/*
 * Iterates over all outputs and pushes sticky windows to the currently visible
 * workspace on that output.
 *
 */
void output_push_sticky_windows(Con *to_focus) {
    Con *output;
    TAILQ_FOREACH(output, &(croot->focus_head), focused) {
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
                if (current->type != CT_FLOATING_CON)
                    continue;

                if (con_is_sticky(current)) {
                    bool ignore_focus = (to_focus == NULL) || (current != to_focus->parent);
                    con_move_to_workspace(current, visible_ws, true, false, ignore_focus);
                }
            }
        }
    }
}
