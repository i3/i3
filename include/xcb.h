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

#include "data.h"

#define _NET_WM_STATE_REMOVE    0
#define _NET_WM_STATE_ADD       1
#define _NET_WM_STATE_TOGGLE    2

/* This is the equivalent of XC_left_ptr. I’m not sure why xcb doesn’t have a constant for that. */
#define XCB_CURSOR_LEFT_PTR     68
#define XCB_CURSOR_SB_H_DOUBLE_ARROW 108
#define XCB_CURSOR_SB_V_DOUBLE_ARROW 116

enum { _NET_SUPPORTED = 0,
        _NET_SUPPORTING_WM_CHECK,
        _NET_WM_NAME,
        _NET_WM_STATE_FULLSCREEN,
        _NET_WM_STATE,
        _NET_WM_WINDOW_TYPE,
        _NET_WM_WINDOW_TYPE_DOCK,
        _NET_WM_DESKTOP,
        _NET_WM_STRUT_PARTIAL,
        UTF8_STRING
};

i3Font *load_font(xcb_connection_t *connection, const char *pattern);
uint32_t get_colorpixel(xcb_connection_t *conn, Client *client, xcb_window_t window, char *hex);
xcb_window_t create_window(xcb_connection_t *conn, Rect r, uint16_t window_class, uint16_t cursor,
                           uint32_t mask, uint32_t *values);
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value);
void xcb_draw_line(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t to_x, uint32_t to_y);
void xcb_draw_rect(xcb_connection_t *connection, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

#endif
