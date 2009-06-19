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
#include <xcb/xcb_property.h>
#include <xcb/xcb_event.h>

#include <X11/XKBlib.h>

#include "queue.h"
#include "data.h"

#ifndef _I3_H
#define _I3_H

#define NUM_ATOMS 17

extern char **start_argv;
extern Display *xkbdpy;
extern TAILQ_HEAD(bindings_head, Binding) bindings;
extern TAILQ_HEAD(autostarts_head, Autostart) autostarts;
extern TAILQ_HEAD(assignments_head, Assignment) assignments;
extern SLIST_HEAD(stack_wins_head, Stack_Window) stack_wins;
extern xcb_event_handlers_t evenths;
extern int num_screens;
extern xcb_atom_t atoms[NUM_ATOMS];

#endif
