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

bool focus_window_in_container(xcb_connection_t *conn, Container *container, direction_t direction);

/** Switches to the given workspace */
void show_workspace(xcb_connection_t *conn, int workspace);

/** Parses a command, see file CMDMODE for more information */
void parse_command(xcb_connection_t *conn, const char *command);

#endif
