/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * parser_util.c: utility functions for the config and commands parser
 *
 */
#include "all.h"
#include "parser_util.h"

/*
 * Pushes a string (identified by 'identifier') on the stack. We simply use a
 * single array, since the number of entries we have to store is very small.
 *
 */
void parser_push_string(struct stack *stack, const char *identifier, const char *str) {
    for (int c = 0; c < 10; c++) {
        if (stack->stack[c].identifier != NULL &&
            strcmp(stack->stack[c].identifier, identifier) != 0) {
            continue;
        }
        if (stack->stack[c].identifier == NULL) {
            /* Found a free slot, let's store it here. */
            stack->stack[c].identifier = identifier;
            stack->stack[c].val.str = sstrdup(str);
            stack->stack[c].type = STACK_STR;
        } else {
            /* Append the value. */
            char *prev = stack->stack[c].val.str;
            sasprintf(&stack->stack[c].val.str, "%s,%s", prev, str);
            free(prev);
        }
        return;
    }

    /* When we arrive here, the stack is full. This should not happen and
     * means there's either a bug in this parser or the specification
     * contains a command with more than 10 identified tokens. */
    fprintf(stderr, "BUG: parser stack full. This means either a bug "
                    "in the code, or a new command which contains more than "
                    "10 identified tokens.\n");
    exit(EXIT_FAILURE);
}

void parser_push_long(struct stack *stack, const char *identifier, const long num) {
    for (int c = 0; c < 10; c++) {
        if (stack->stack[c].identifier != NULL) {
            continue;
        }
        /* Found a free slot, let's store it here. */
        stack->stack[c].identifier = identifier;
        stack->stack[c].val.num = num;
        stack->stack[c].type = STACK_LONG;
        return;
    }

    /* When we arrive here, the stack is full. This should not happen and
     * means there's either a bug in this parser or the specification
     * contains a command with more than 10 identified tokens. */
    fprintf(stderr, "BUG: parser stack full. This means either a bug "
                    "in the code, or a new command which contains more than "
                    "10 identified tokens.\n");
    exit(EXIT_FAILURE);
}

const char *parser_get_string(const struct stack *stack, const char *identifier) {
    for (int c = 0; c < 10; c++) {
        if (stack->stack[c].identifier == NULL) {
            break;
        }
        if (strcmp(identifier, stack->stack[c].identifier) == 0) {
            return stack->stack[c].val.str;
        }
    }
    return NULL;
}

long parser_get_long(const struct stack *stack, const char *identifier) {
    for (int c = 0; c < 10; c++) {
        if (stack->stack[c].identifier == NULL) {
            break;
        }
        if (strcmp(identifier, stack->stack[c].identifier) == 0) {
            return stack->stack[c].val.num;
        }
    }
    return 0;
}

void parser_clear_stack(struct stack *stack) {
    for (int c = 0; c < 10; c++) {
        if (stack->stack[c].type == STACK_STR) {
            free(stack->stack[c].val.str);
        }
        stack->stack[c].identifier = NULL;
        stack->stack[c].val.str = NULL;
        stack->stack[c].val.num = 0;
    }
}
