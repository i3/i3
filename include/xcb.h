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

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

enum { _NET_SUPPORTED = 0,
        _NET_SUPPORTING_WM_CHECK = 1,
        _NET_WM_NAME = 2,
        _NET_WM_STATE_FULLSCREEN = 3,
        _NET_WM_STATE = 4,
        UTF8_STRING = 5
};

uint32_t get_colorpixel(xcb_connection_t *conn, xcb_window_t window, char *hex);
xcb_window_t create_window(xcb_connection_t *conn, Rect r, uint16_t window_class, uint32_t mask, uint32_t *values);

#endif
