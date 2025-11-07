/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * parser_util.h: utility functions for the config and commands parser
 *
 */
#pragma once

struct stack_entry {
    /* Just a pointer, not dynamically allocated. */
    const char *identifier;
    enum {
        STACK_STR = 0,
        STACK_LONG = 1,
    } type;
    union {
        char *str;
        long num;
    } val;
};

struct stack {
    struct stack_entry stack[10];
};

/**
 * Pushes a string (identified by 'identifier') on the stack.
 * If a string with the same identifier is already on the stack, the new
 * string will be appended, separated by a comma.
 *
 */
void parser_push_string(struct stack *stack, const char *identifier, const char *str);

/**
 * Pushes a long (identified by 'identifier') on the stack.
 *
 */
void parser_push_long(struct stack *stack, const char *identifier, long num);

/**
 * Returns the string with the given identifier.
 *
 */
const char *parser_get_string(const struct stack *stack, const char *identifier);

/**
 * Returns the long with the given identifier.
 *
 */
long parser_get_long(const struct stack *stack, const char *identifier);

/**
 * Clears the stack.
 *
 */
void parser_clear_stack(struct stack *stack);
