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

#ifndef _RESIZE_H
#define _RESIZE_H

#include <xcb/xcb.h>

typedef enum { O_HORIZONTAL, O_VERTICAL } resize_orientation_t;

/**
 * Renders the resize window between the first/second container and resizes
 * the table column/row.
 *
 */
int resize_graphical_handler(xcb_connection_t *conn, Workspace *ws, int first,
                             int second, resize_orientation_t orientation,
                             xcb_button_press_event_t *event);

#endif
