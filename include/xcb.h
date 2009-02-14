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
#ifndef _XCB_H
#define _XCB_H

uint32_t get_colorpixel(xcb_connection_t *conn, xcb_window_t window, char *hex);

#endif
