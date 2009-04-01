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
#include <xcb/xcb.h>

#ifndef _COMMANDS_H
#define _COMMANDS_H

bool focus_window_in_container(xcb_connection_t *conn, Container *container, direction_t direction);
void show_workspace(xcb_connection_t *conn, int workspace);
void parse_command(xcb_connection_t *conn, const char *command);

#endif
