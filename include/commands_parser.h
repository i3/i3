/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * commands.c: all command functions (see commands_parser.c)
 *
 */
#ifndef _COMMANDS_PARSER_H
#define _COMMANDS_PARSER_H

#include <yajl/yajl_gen.h>

/*
 * Holds the result of a call to any command. When calling
 * parse_command("floating enable, border none"), the parser will internally
 * use a struct CommandResult when calling cmd_floating and cmd_border.
 * parse_command will also return another struct CommandResult, whose
 * json_output is set to a map of individual json_outputs and whose
 * needs_tree_trender is true if any individual needs_tree_render was true.
 *
 */
struct CommandResult {
    /* The JSON generator to append a reply to. */
    yajl_gen json_gen;

    /* Whether the command requires calling tree_render. */
    bool needs_tree_render;
};

struct CommandResult *parse_command(const char *input);

#endif
