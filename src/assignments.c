/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * assignments.c: Assignments for specific windows (for_window).
 *
 */
#include "all.h"

/*
 * Checks the list of assignments for the given window and runs all matching
 * ones (unless they have already been run for this specific window).
 *
 */
void run_assignments(i3Window *window) {
    DLOG("Checking if any assignments match this window\n");

    bool needs_tree_render = false;

    /* Check if any assignments match */
    Assignment *current;
    TAILQ_FOREACH(current, &assignments, assignments) {
        if (!match_matches_window(&(current->match), window))
            continue;

        bool skip = false;
        for (int c = 0; c < window->nr_assignments; c++) {
            if (window->ran_assignments[c] != current)
                continue;

            DLOG("This assignment already ran for the given window, not executing it again.\n");
            skip = true;
            break;
        }

        if (skip)
            continue;

        DLOG("matching assignment, would do:\n");
        if (current->type == A_COMMAND) {
            DLOG("execute command %s\n", current->dest.command);
            char *full_command;
            sasprintf(&full_command, "[id=\"%d\"] %s", window->id, current->dest.command);
            struct CommandResult *command_output = parse_command(full_command);
            free(full_command);

            if (command_output->needs_tree_render)
                needs_tree_render = true;

            yajl_gen_free(command_output->json_gen);
        }

        /* Store that we ran this assignment to not execute it again */
        window->nr_assignments++;
        window->ran_assignments = srealloc(window->ran_assignments, sizeof(Assignment*) * window->nr_assignments);
        window->ran_assignments[window->nr_assignments-1] = current;
    }

    /* If any of the commands required re-rendering, we will do that now. */
    if (needs_tree_render)
        tree_render();
}

/*
 * Returns the first matching assignment for the given window.
 *
 */
Assignment *assignment_for(i3Window *window, int type) {
    Assignment *assignment;

    TAILQ_FOREACH(assignment, &assignments, assignments) {
        if ((type != A_ANY && (assignment->type & type) == 0) ||
            !match_matches_window(&(assignment->match), window))
            continue;
        DLOG("got a matching assignment (to %s)\n", assignment->dest.workspace);
        return assignment;
    }

    return NULL;
}
