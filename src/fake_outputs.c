#undef I3__FILE__
#define I3__FILE__ "fake_outputs.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * Faking outputs is useful in pathological situations (like network X servers
 * which don’t support multi-monitor in a useful way) and for our testsuite.
 *
 */
#include "all.h"

static int num_screens;

/*
 * Looks in outputs for the Output whose start coordinates are x, y
 *
 */
static Output *get_screen_at(unsigned int x, unsigned int y) {
    Output *output;
    TAILQ_FOREACH (output, &outputs, outputs)
        if (output->rect.x == x && output->rect.y == y)
            return output;

    return NULL;
}

/*
 * Creates outputs according to the given specification.
 * The specification must be in the format wxh+x+y, for example 1024x768+0+0,
 * with multiple outputs separated by commas:
 *   1900x1200+0+0,1280x1024+1900+0
 *
 */
void fake_outputs_init(const char *output_spec) {
    char useless_buffer[1024];
    const char *walk = output_spec;
    unsigned int x, y, width, height;
    while (sscanf(walk, "%ux%u+%u+%u", &width, &height, &x, &y) == 4) {
        DLOG("Parsed output as width = %u, height = %u at (%u, %u)\n",
             width, height, x, y);
        Output *new_output = get_screen_at(x, y);
        if (new_output != NULL) {
            DLOG("Re-used old output %p\n", new_output);
            /* This screen already exists. We use the littlest screen so that the user
               can always see the complete workspace */
            new_output->rect.width = min(new_output->rect.width, width);
            new_output->rect.height = min(new_output->rect.height, height);
        } else {
            new_output = scalloc(sizeof(Output));
            sasprintf(&(new_output->name), "fake-%d", num_screens);
            DLOG("Created new fake output %s (%p)\n", new_output->name, new_output);
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
            init_ws_for_output(new_output, output_get_content(new_output->con));
            num_screens++;
        }

        /* Figure out how long the input was to skip it */
        walk += sprintf(useless_buffer, "%ux%u+%u+%u", width, height, x, y) + 1;
    }

    if (num_screens == 0) {
        ELOG("No screens found. Please fix your setup. i3 will exit now.\n");
        exit(0);
    }
}
