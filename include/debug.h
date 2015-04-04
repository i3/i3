/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * debug.c: Debugging functions, especially FormatEvent, which prints unhandled
 *          events.  This code is from xcb-util.
 *
 */
#pragma once

int handle_event(void *ignored, xcb_connection_t *c, xcb_generic_event_t *e);
