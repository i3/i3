/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * config_parser.h: config parser-related definitions
 *
 */
#ifndef I3_CONFIG_PARSER_H
#define I3_CONFIG_PARSER_H

#include <yajl/yajl_gen.h>

/*
 * The result of a parse_config call. Currently unused, but the JSON output
 * will be useful in the future when we implement a config parsing IPC command.
 *
 */
struct ConfigResult {
    /* The JSON generator to append a reply to. */
    yajl_gen json_gen;

    /* The next state to transition to. Passed to the function so that we can
     * determine the next state as a result of a function call, like
     * cfg_criteria_pop_state() does. */
    int next_state;
};

struct ConfigResult *parse_config(const char *input, struct context *context);

#endif
