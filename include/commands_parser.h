/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * commands.c: all command functions (see commands_parser.c)
 *
 */
#pragma once

#include <config.h>

#include <yajl/yajl_gen.h>
#include "parser_util.h"
#include "queue.h"

/**
 * Helper data structure for an operation window (window on which the operation
 * will be performed). Used to build the TAILQ owindows.
 *
 */
typedef struct owindow {
    Con *con;
    TAILQ_ENTRY(owindow) owindows;
} owindow;

/**
 * Holds an intermediate representation of the result of a call to any command.
 * When calling parse_command("floating enable, border none"), the parser will
 * internally use this struct when calling cmd_floating and cmd_border.
 */
struct CommandResultIR {
    /* The parser context this command is executing in. */
    struct cmd_parser_ctx *ctx;

    /* The JSON generator to append a reply to (may be NULL). */
    yajl_gen json_gen;

    /* The IPC client connection which sent this command (may be NULL, e.g. for
       key bindings). */
    ipc_client *client;

    /* The next state to transition to. Passed to the function so that we can
     * determine the next state as a result of a function call, like
     * cfg_criteria_pop_state() does. */
    int next_state;

    /* Whether the command requires calling tree_render. */
    bool needs_tree_render;
};

/* Define the owindows head structure here so it's complete */
TAILQ_HEAD(owindows_head, owindow);

/**
 * Context structure for the command parser, making it re-entrant.
 */
struct cmd_parser_ctx {
    int state;
    Match current_match;

    /* The (small) stack where identified literals are stored during the parsing
     * of a single command (like $workspace). */
    struct stack stack;

    /* List of operation windows (windows on which operations will be performed).
     * Used to build the TAILQ owindows. */
    struct owindows_head owindows;

    struct CommandResultIR subcommand_output;
    struct CommandResultIR command_output;
};

typedef struct CommandResult CommandResult;

/**
 * A struct that contains useful information about the result of a command as a
 * whole (e.g. a compound command like "floating enable, border none").
 * needs_tree_render is true if needs_tree_render of any individual command was
 * true.
 */
struct CommandResult {
    bool parse_error;
    /* the error_message is currently only set for parse errors */
    char *error_message;
    bool needs_tree_render;
};

/**
 * Parses a string (or word, if as_word is true). Extracted out of
 * parse_command so that it can be used in src/workspace.c for interpreting
 * workspace commands.
 *
 */
char *parse_string(const char **walk, bool as_word);

/**
 * Parses and executes the given command. If a caller-allocated yajl_gen is
 * passed, a json reply will be generated in the format specified by the ipc
 * protocol. Pass NULL if no json reply is required.
 *
 * Free the returned CommandResult with command_result_free().
 */
CommandResult *parse_command(const char *input, yajl_gen gen, ipc_client *client);

/**
 * Frees a CommandResult
 */
void command_result_free(CommandResult *result);
