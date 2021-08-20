/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * xcursor.c: xcursor support for themed cursors.
 *
 */
#include <config.h>

#include <config.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>

#include "libi3.h"
#include "data.h"
#include "util.h"
#include "ipc.h"
#include "tree.h"
#include "log.h"
#include "xcb.h"
#include "manage.h"
#include "workspace.h"
#include "i3.h"
#include "x.h"
#include "click.h"
#include "key_press.h"
#include "floating.h"
#include "drag.h"
#include "configuration.h"
#include "handlers.h"
#include "randr.h"
#include "xinerama.h"
#include "con.h"
#include "load_layout.h"
#include "render.h"
#include "window.h"
#include "match.h"
#include "xcursor.h"
#include "resize.h"
#include "sighandler.h"
#include "move.h"
#include "output.h"
#include "ewmh.h"
#include "assignments.h"
#include "regex.h"
#include "startup.h"
#include "scratchpad.h"
#include "commands.h"
#include "commands_parser.h"
#include "bindings.h"
#include "config_directives.h"
#include "config_parser.h"
#include "fake_outputs.h"
#include "display_version.h"
#include "restore_layout.h"
#include "sync.h"
#include "main.h"

#include <assert.h>
#include <err.h>

#include <xcb/xcb_cursor.h>

static xcb_cursor_context_t *ctx;
static xcb_cursor_t cursors[XCURSOR_CURSOR_MAX];

void xcursor_load_cursors(void) {
    if (xcb_cursor_context_new(conn, root_screen, &ctx) < 0) {
        errx(EXIT_FAILURE, "Cannot allocate xcursor context");
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
