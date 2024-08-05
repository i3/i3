/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * handlers.c: Small handlers for various events (keypresses, focus changes,
 *             …).
 *
 */
#pragma once

#include <config.h>

#include <xcb/randr.h>

extern int randr_base;
extern int xkb_base;
extern int shape_base;

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
