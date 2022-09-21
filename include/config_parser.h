/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * config_parser.h: config parser-related definitions
 *
 */
#pragma once

#include <config.h>

#include <yajl/yajl_gen.h>

SLIST_HEAD(variables_head, Variable);
extern pid_t config_error_nagbar_pid;

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

struct parser_ctx {
    bool use_nagbar;
    bool assume_v4;

    int state;
    Match current_match;

    /* A list which contains the states that lead to the current state, e.g.
     * INITIAL, WORKSPACE_LAYOUT.
     * When jumping back to INITIAL, statelist_idx will simply be set to 1
     * (likewise for other states, e.g. MODE or BAR).
     * This list is used to process the nearest error token. */
    int statelist[10];
    /* NB: statelist_idx points to where the next entry will be inserted */
    int statelist_idx;

    /*******************************************************************************
     * The (small) stack where identified literals are stored during the parsing
     * of a single config directive (like $workspace).
     ******************************************************************************/
    struct stack *stack;

    struct variables_head variables;

    bool has_errors;
};

/**
 * An intermediate representation of the result of a parse_config call.
 * Currently unused, but the JSON output will be useful in the future when we
 * implement a config parsing IPC command.
 *
 */
struct ConfigResultIR {
    struct parser_ctx *ctx;

    /* The next state to transition to. Passed to the function so that we can
     * determine the next state as a result of a function call, like
     * cfg_criteria_pop_state() does. */
    int next_state;

    /* Whether any error happened while processing this config directive. */
    bool has_errors;
};

/**
 * launch nagbar to indicate errors in the configuration file.
 */
void start_config_error_nagbar(const char *configpath, bool has_errors);

/**
 * Releases the memory of all variables in ctx.
 *
 */
void free_variables(struct parser_ctx *ctx);

typedef enum {
    PARSE_FILE_FAILED = -1,
    PARSE_FILE_SUCCESS = 0,
    PARSE_FILE_CONFIG_ERRORS = 1,
} parse_file_result_t;

/**
 * Parses the given file by first replacing the variables, then calling
 * parse_config and launching i3-nagbar if use_nagbar is true.
 *
 * The return value is a boolean indicating whether there were errors during
 * parsing.
 *
 */
parse_file_result_t parse_file(struct parser_ctx *ctx, const char *f, IncludedFile *included_file);
