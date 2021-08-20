/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * Faking outputs is useful in pathological situations (like network X servers
 * which don’t support multi-monitor in a useful way) and for our testsuite.
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

static int num_screens;

/*
 * Looks in outputs for the Output whose start coordinates are x, y
 *
 */
static Output *get_screen_at(unsigned int x, unsigned int y) {
    Output *output;
    TAILQ_FOREACH (output, &outputs, outputs) {
        if (output->rect.x == x && output->rect.y == y) {
            return output;
        }
    }

    return NULL;
}

/*
 * Creates outputs according to the given specification.
 * The specification must be in the format wxh+x+y, for example 1024x768+0+0,
 * optionally followed by 'P' to indicate a primary output,
 * with multiple outputs separated by commas:
 *   1900x1200+0+0P,1280x1024+1900+0
 *
 */
void fake_outputs_init(const char *output_spec) {
    const char *walk = output_spec;
    unsigned int x, y, width, height;
    int chars_consumed;
    while (sscanf(walk, "%ux%u+%u+%u%n", &width, &height, &x, &y, &chars_consumed) == 4) {
        walk += chars_consumed;
        bool primary = false;
        if (*walk == 'P') {
            primary = true;
            walk++;
        }
        if (*walk == ',')
            walk++; /* Skip delimiter */
        DLOG("Parsed output as width = %u, height = %u at (%u, %u)%s\n",
             width, height, x, y, primary ? " (primary)" : "");

        Output *new_output = get_screen_at(x, y);
        if (new_output != NULL) {
            DLOG("Re-used old output %p\n", new_output);
            /* This screen already exists. We use the littlest screen so that the user
               can always see the complete workspace */
            new_output->rect.width = min(new_output->rect.width, width);
            new_output->rect.height = min(new_output->rect.height, height);
        } else {
            struct output_name *output_name = scalloc(1, sizeof(struct output_name));
            new_output = scalloc(1, sizeof(Output));
            sasprintf(&(output_name->name), "fake-%d", num_screens);
            SLIST_INIT(&(new_output->names_head));
            SLIST_INSERT_HEAD(&(new_output->names_head), output_name, names);
            DLOG("Created new fake output %s (%p)\n", output_primary_name(new_output), new_output);
            new_output->active = true;
            new_output->rect.x = x;
            new_output->rect.y = y;
            new_output->rect.width = width;
            new_output->rect.height = height;
            /* We always treat the screen at 0x0 as the primary screen */
            if (new_output->rect.x == 0 && new_output->rect.y == 0)
                TAILQ_INSERT_HEAD(&outputs, new_output, outputs);
            else
                TAILQ_INSERT_TAIL(&outputs, new_output, outputs);
            output_init_con(new_output);
            init_ws_for_output(new_output);
            num_screens++;
        }
        new_output->primary = primary;
    }

    if (num_screens == 0) {
        ELOG("No screens found. Please fix your setup. i3 will exit now.\n");
        exit(EXIT_FAILURE);
    }
}
