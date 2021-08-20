/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * assignments.c: Assignments for specific windows (for_window).
 *
 */
#include <config.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>

#include "libi3.h"
#include "data.h"
#include "util.h"
#include "ipc.h"
#include "tree.h"
#include "log.h"
#include "xcb.h"
#include "manage.h"
#include "workspace.h"
#include "i3.h"
#include "x.h"
#include "click.h"
#include "key_press.h"
#include "floating.h"
#include "drag.h"
#include "configuration.h"
#include "handlers.h"
#include "randr.h"
#include "xinerama.h"
#include "con.h"
#include "load_layout.h"
#include "render.h"
#include "window.h"
#include "match.h"
#include "xcursor.h"
#include "resize.h"
#include "sighandler.h"
#include "move.h"
#include "output.h"
#include "ewmh.h"
#include "assignments.h"
#include "regex.h"
#include "startup.h"
#include "scratchpad.h"
#include "commands.h"
#include "commands_parser.h"
#include "bindings.h"
#include "config_directives.h"
#include "config_parser.h"
#include "fake_outputs.h"
#include "display_version.h"
#include "restore_layout.h"
#include "sync.h"
#include "main.h"

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
    TAILQ_FOREACH (current, &assignments, assignments) {
        if (current->type != A_COMMAND || !match_matches_window(&(current->match), window))
            continue;

        bool skip = false;
        for (uint32_t c = 0; c < window->nr_assignments; c++) {
            if (window->ran_assignments[c] != current)
                continue;

            DLOG("This assignment already ran for the given window, not executing it again.\n");
            skip = true;
            break;
        }

        if (skip)
            continue;

        /* Store that we ran this assignment to not execute it again. We have
         * to do this before running the actual command to prevent infinite
         * loops. */
        window->nr_assignments++;
        window->ran_assignments = srealloc(window->ran_assignments, sizeof(Assignment *) * window->nr_assignments);
        window->ran_assignments[window->nr_assignments - 1] = current;

        DLOG("matching assignment, execute command %s\n", current->dest.command);
        char *full_command;
        sasprintf(&full_command, "[id=\"%d\"] %s", window->id, current->dest.command);
        CommandResult *result = parse_command(full_command, NULL, NULL);
        free(full_command);

        if (result->needs_tree_render)
            needs_tree_render = true;

        command_result_free(result);
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

    TAILQ_FOREACH (assignment, &assignments, assignments) {
        if ((type != A_ANY && (assignment->type & type) == 0) ||
            !match_matches_window(&(assignment->match), window))
            continue;
        DLOG("got a matching assignment\n");
        return assignment;
    }

    return NULL;
}
