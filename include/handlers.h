/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * handlers.c: Small handlers for various events (keypresses, focus changes,
 *             …).
 *
 */
#pragma once

#include <xcb/randr.h>

extern int randr_base;
extern int xkb_base;

/**
 * Adds the given sequence to the list of events which are ignored.
 * If this ignore should only affect a specific response_type, pass
 * response_type, otherwise, pass -1.
 *
 * Every ignored sequence number gets garbage collected after 5 seconds.
 *
 */
void add_ignore_event(const int sequence, const int response_type);

/**
 * Checks if the given sequence is ignored and returns true if so.
 *
 */
bool event_is_ignored(const int sequence, const int response_type);

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
void property_handlers_init(void);

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
