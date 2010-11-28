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

#endif
