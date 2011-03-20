/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _HANDLERS_H
#define _HANDLERS_H

#include <xcb/randr.h>

extern int randr_base;

void add_ignore_event(const int sequence);

/**
 * Takes an xcb_generic_event_t and calls the appropriate handler, based on the
 * event type.
 *
 */
void handle_event(int type, xcb_generic_event_t *event);

/**
 * Sets the appropriate atoms for the property handlers after the atoms were
 * received from X11
 *
 */
void property_handlers_init();

#if 0
/**
 * Configuration notifies are only handled because we need to set up ignore
 * for the following enter notify events
 *
 */
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event);
#endif

#if 0
/**
 * Handles _NET_WM_WINDOW_TYPE changes
 *
 */
int handle_window_type(void *data, xcb_connection_t *conn, uint8_t state,
                       xcb_window_t window, xcb_atom_t atom,
                       xcb_get_property_reply_t *property);
#endif

#endif
