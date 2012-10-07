/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * commands.c: all command functions (see commands_parser.c)
 *
 */
#ifndef I3_CONFIG_DIRECTIVES_H
#define I3_CONFIG_DIRECTIVES_H

//#include "config_parser.h"

/** The beginning of the prototype for every cmd_ function. */
#define I3_CFG Match *current_match, struct CommandResult *cmd_output

/**
 *
 */
void cfg_font(I3_CFG, const char *font);

void cfg_mode_binding(I3_CFG, const char *bindtype, const char *modifiers, const char *key, const char *command);

void cfg_enter_mode(I3_CFG, const char *mode);

void cfg_exec(I3_CFG, const char *exectype, const char *no_startup_id, const char *command);

#endif
