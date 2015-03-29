#undef I3__FILE__
#define I3__FILE__ "output.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2013 Michael Stapelberg and contributors (see also: LICENSE)
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
    Output *output;

    if (strcasecmp(output_str, "left") == 0)
        output = get_output_next_wrap(D_LEFT, current_output);
    else if (strcasecmp(output_str, "right") == 0)
        output = get_output_next_wrap(D_RIGHT, current_output);
    else if (strcasecmp(output_str, "up") == 0)
        output = get_output_next_wrap(D_UP, current_output);
    else if (strcasecmp(output_str, "down") == 0)
        output = get_output_next_wrap(D_DOWN, current_output);
    else
        output = get_output_by_name(output_str);

    return output;
}
