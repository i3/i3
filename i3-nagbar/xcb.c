/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <X11/keysym.h>

#include "i3-nagbar.h"

/*
 * Opens the window we use for input/output and maps it
 *
 */
xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height) {
        xcb_window_t win = xcb_generate_id(conn);
        //xcb_cursor_t cursor_id = xcb_generate_id(conn);

#if 0
        /* Use the default cursor (left pointer) */
        if (cursor > -1) {
                i3Font *cursor_font = load_font(conn, "cursor");
                xcb_create_glyph_cursor(conn, cursor_id, cursor_font->id, cursor_font->id,
                                XCB_CURSOR_LEFT_PTR, XCB_CURSOR_LEFT_PTR + 1,
                                0, 0, 0, 65535, 65535, 65535);
        }
#endif

        uint32_t mask = 0;
        uint32_t values[3];

        mask |= XCB_CW_BACK_PIXEL;
        values[0] = 0;

	mask |= XCB_CW_EVENT_MASK;
	values[1] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE;

        xcb_create_window(conn,
                          XCB_COPY_FROM_PARENT,
                          win, /* the window id */
                          root, /* parent == root */
                          50, 50, width, height, /* dimensions */
                          0, /* border = 0, we draw our own */
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                          mask,
                          values);

#if 0
        if (cursor > -1)
                xcb_change_window_attributes(conn, result, XCB_CW_CURSOR, &cursor_id);
#endif

        /* Map the window (= make it visible) */
        xcb_map_window(conn, win);

	return win;
}
