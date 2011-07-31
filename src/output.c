/*
 * vim:ts=4:sw=4:expandtab
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

    ELOG("output_get_content() called on non-output %p\n", output);
    assert(false);
}
