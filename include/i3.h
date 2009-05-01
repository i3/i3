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
#include <xcb/xcb_property.h>
#include <xcb/xcb_event.h>

#include <X11/XKBlib.h>

#include "queue.h"
#include "data.h"

#ifndef _I3_H
#define _I3_H

#define NUM_ATOMS 12

extern char **start_argv;
extern Display *xkbdpy;
extern TAILQ_HEAD(bindings_head, Binding) bindings;
extern SLIST_HEAD(stack_wins_head, Stack_Window) stack_wins;
extern xcb_event_handlers_t evenths;
extern int num_screens;
extern xcb_atom_t atoms[NUM_ATOMS];

void manage_window(xcb_property_handlers_t *prophs, xcb_connection_t *conn, xcb_window_t window, window_attributes_t wa);
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
                     xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
                     int16_t x, int16_t y, uint16_t width, uint16_t height);

#endif
