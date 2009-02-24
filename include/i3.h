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
#include <xcb/xcb_event.h>

#include <X11/XKBlib.h>

#include "queue.h"

#ifndef _I3_H
#define _I3_H

#define NUM_ATOMS 9

extern Display *xkbdpy;
extern TAILQ_HEAD(bindings_head, Binding) bindings;
extern SLIST_HEAD(stack_wins_head, Stack_Window) stack_wins;
extern xcb_event_handlers_t evenths;
extern char *pattern;
extern int num_screens;
extern xcb_atom_t atoms[NUM_ATOMS];

#endif
