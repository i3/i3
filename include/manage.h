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

#ifndef _MANAGE_H
#define _MANAGE_H

/**
 * Go through all existing windows (if the window manager is restarted) and manage them
 *
 */
void manage_existing_windows(xcb_connection_t *conn, xcb_property_handlers_t *prophs, xcb_window_t root);

/**
 * Do some sanity checks and then reparent the window.
 *
 */
void manage_window(xcb_property_handlers_t *prophs, xcb_connection_t *conn,
                   xcb_window_t window, window_attributes_t wa);

/**
 * reparent_window() gets called when a new window was opened and becomes a child of the root
 * window, or it gets called by us when we manage the already existing windows at startup.
 *
 * Essentially, this is the point where we take over control.
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
                     xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
                     int16_t x, int16_t y, uint16_t width, uint16_t height);

#endif
