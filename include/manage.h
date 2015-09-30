/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * manage.c: Initially managing new windows (or existing ones on restart).
 *
 */
#pragma once

#include "data.h"

/**
 * Go through all existing windows (if the window manager is restarted) and
 * manage them
 *
 */
void manage_existing_windows(xcb_window_t root);

/**
 * Restores the geometry of each window by reparenting it to the root window
 * at the position of its frame.
 *
 * This is to be called *only* before exiting/restarting i3 because of evil
 * side-effects which are to be expected when continuing to run i3.
 *
 */
void restore_geometry(void);

/**
 * Do some sanity checks and then reparent the window.
 *
 */
void manage_window(xcb_window_t window,
                   xcb_get_window_attributes_cookie_t cookie,
                   bool needs_to_be_mapped);

#if 0
/**
 * reparent_window() gets called when a new window was opened and becomes a
 * child of the root window, or it gets called by us when we manage the
 * already existing windows at startup.
 *
 * Essentially, this is the point where we take over control.
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
                     xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
                     int16_t x, int16_t y, uint16_t width, uint16_t height,
                     uint32_t border_width);

#endif
