/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "all.h"

/*
 * Checks the list of assignments for the given window and runs all matching
 * ones (unless they have already been run for this specific window).
 *
 */
void run_assignments(i3Window *window) {
    DLOG("Checking assignments...\n");

    /* Check if any assignments match */
    Assignment *current;
    TAILQ_FOREACH(current, &real_assignments, real_assignments) {
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
            asprintf(&full_command, "[id=\"%d\"] %s", window->id, current->dest.command);
            parse_cmd(full_command);
        }

        /* Store that we ran this assignment to not execute it again */
        window->nr_assignments++;
        window->ran_assignments = srealloc(window->ran_assignments, sizeof(Assignment*) * window->nr_assignments);
        window->ran_assignments[window->nr_assignments-1] = current;
    }
}
