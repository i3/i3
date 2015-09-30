#undef I3__FILE__
#define I3__FILE__ "xcursor.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * xcursor.c: xcursor support for themed cursors.
 *
 */
#include <assert.h>
#include <xcb/xcb_cursor.h>

#include "i3.h"
#include "xcb.h"
#include "xcursor.h"

static xcb_cursor_context_t *ctx;
static xcb_cursor_t cursors[XCURSOR_CURSOR_MAX];

static const int xcb_cursors[XCURSOR_CURSOR_MAX] = {
    XCB_CURSOR_LEFT_PTR,
    XCB_CURSOR_SB_H_DOUBLE_ARROW,
    XCB_CURSOR_SB_V_DOUBLE_ARROW,
    XCB_CURSOR_WATCH};

void xcursor_load_cursors(void) {
    if (xcb_cursor_context_new(conn, root_screen, &ctx) < 0) {
        ELOG("xcursor support unavailable\n");
        xcursor_supported = false;
        return;
    }
#define LOAD_CURSOR(constant, name)                            \
    do {                                                       \
        cursors[constant] = xcb_cursor_load_cursor(ctx, name); \
    } while (0)
    LOAD_CURSOR(XCURSOR_CURSOR_POINTER, "left_ptr");
    LOAD_CURSOR(XCURSOR_CURSOR_RESIZE_HORIZONTAL, "sb_h_double_arrow");
    LOAD_CURSOR(XCURSOR_CURSOR_RESIZE_VERTICAL, "sb_v_double_arrow");
    LOAD_CURSOR(XCURSOR_CURSOR_WATCH, "watch");
    LOAD_CURSOR(XCURSOR_CURSOR_MOVE, "fleur");
    LOAD_CURSOR(XCURSOR_CURSOR_TOP_LEFT_CORNER, "top_left_corner");
    LOAD_CURSOR(XCURSOR_CURSOR_TOP_RIGHT_CORNER, "top_right_corner");
    LOAD_CURSOR(XCURSOR_CURSOR_BOTTOM_LEFT_CORNER, "bottom_left_corner");
    LOAD_CURSOR(XCURSOR_CURSOR_BOTTOM_RIGHT_CORNER, "bottom_right_corner");
#undef LOAD_CURSOR
}

/*
 * Sets the cursor of the root window to the 'pointer' cursor.
 *
 * This function is called when i3 is initialized, because with some login
 * managers, the root window will not have a cursor otherwise.
 *
 */
void xcursor_set_root_cursor(int cursor_id) {
    xcb_change_window_attributes(conn, root, XCB_CW_CURSOR,
                                 (uint32_t[]){xcursor_get_cursor(cursor_id)});
}

xcb_cursor_t xcursor_get_cursor(enum xcursor_cursor_t c) {
    assert(c < XCURSOR_CURSOR_MAX);
    return cursors[c];
}

int xcursor_get_xcb_cursor(enum xcursor_cursor_t c) {
    assert(c < XCURSOR_CURSOR_MAX);
    return xcb_cursors[c];
}
