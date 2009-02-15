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

#include "data.h"

#ifndef _UTIL_H
#define _UTIL_H

#define exit_if_null(pointer, ...) { if (pointer == NULL) die(__VA_ARGS__); }

int min(int a, int b);
int max(int a, int b);
void die(char *fmt, ...);
void *smalloc(size_t size);
char *sstrdup(const char *str);
void start_application(const char *command);
void check_error(xcb_connection_t *connection, xcb_void_cookie_t cookie, char *err_message);
void set_focus(xcb_connection_t *conn, Client *client);
void warp_pointer_into(xcb_connection_t *connection, Client *client);
void toggle_fullscreen(xcb_connection_t *conn, Client *client);

#endif
