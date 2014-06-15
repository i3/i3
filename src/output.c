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

    TAILQ_FOREACH (child, &(output->nodes_head), nodes)
        if (child->type == CT_CON)
            return child;

    return NULL;
}
