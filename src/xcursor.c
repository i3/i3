#include <assert.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/cursorfont.h>

#include "i3.h"
#include "xcb.h"
#include "xcursor.h"

static Cursor cursors[XCURSOR_CURSOR_MAX];

static const int xcb_cursors[XCURSOR_CURSOR_MAX] = {
    XCB_CURSOR_LEFT_PTR,
    XCB_CURSOR_SB_H_DOUBLE_ARROW,
    XCB_CURSOR_SB_V_DOUBLE_ARROW
};

static Cursor load_cursor(const char *name, int font)
{
    Cursor c = XcursorLibraryLoadCursor(xlibdpy, name);
    if (c == None)
        c = XCreateFontCursor(xlibdpy, font);
    return c;
}

void xcursor_load_cursors()
{
    cursors[XCURSOR_CURSOR_POINTER] = load_cursor("left_ptr", XC_left_ptr);
    cursors[XCURSOR_CURSOR_RESIZE_HORIZONTAL] = load_cursor("sb_h_double_arrow", XC_sb_h_double_arrow);
    cursors[XCURSOR_CURSOR_RESIZE_VERTICAL] = load_cursor("sb_v_double_arrow", XC_sb_v_double_arrow);
}

Cursor xcursor_get_cursor(enum xcursor_cursor_t c)
{
    assert(c >= 0 && c < XCURSOR_CURSOR_MAX);
    return cursors[c];
}

int xcursor_get_xcb_cursor(enum xcursor_cursor_t c)
{
    assert(c >= 0 && c < XCURSOR_CURSOR_MAX);
    return xcb_cursors[c];
}
