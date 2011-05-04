/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <X11/XKBlib.h>

#include "queue.h"
#include "data.h"

#ifndef _I3_H
#define _I3_H

extern xcb_connection_t *global_conn;
extern xcb_key_symbols_t *keysyms;
extern char **start_argv;
extern Display *xkbdpy;
extern int xkb_current_group;
extern TAILQ_HEAD(bindings_head, Binding) *bindings;
extern TAILQ_HEAD(autostarts_head, Autostart) autostarts;
extern TAILQ_HEAD(assignments_head, Assignment) assignments;
extern SLIST_HEAD(stack_wins_head, Stack_Window) stack_wins;
extern uint8_t root_depth;
extern bool xkb_supported;
extern xcb_window_t root;

#endif
