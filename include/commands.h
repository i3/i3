/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _COMMANDS_H
#define _COMMANDS_H

#include <xcb/xcb.h>

#if 0
bool focus_window_in_container(xcb_connection_t *conn, Container *container,
                               direction_t direction);
#endif

/** Parses a command, see file CMDMODE for more information */
void parse_command(const char *command);

#endif
