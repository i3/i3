/*
 * vim:ts=4:sw=4:expandtab
 */
#ifndef _XCURSOR_CURSOR_H
#define _XCURSOR_CURSOR_H

#include <X11/Xlib.h>

enum xcursor_cursor_t {
    XCURSOR_CURSOR_POINTER = 0,
    XCURSOR_CURSOR_RESIZE_HORIZONTAL,
    XCURSOR_CURSOR_RESIZE_VERTICAL,
    XCURSOR_CURSOR_MAX
};

extern void xcursor_load_cursors();
extern Cursor xcursor_get_cursor(enum xcursor_cursor_t c);
extern int xcursor_get_xcb_cursor(enum xcursor_cursor_t c);

/**
 * Sets the cursor of the root window to the 'pointer' cursor.
 *
 * This function is called when i3 is initialized, because with some login
 * managers, the root window will not have a cursor otherwise.
 *
 * We have a separate xcursor function to use the same X11 connection as the
 * xcursor_load_cursors() function. If we mix the Xlib and the XCB connection,
 * races might occur (even though we flush the Xlib connection).
 *
 */
void xcursor_set_root_cursor();

#endif
