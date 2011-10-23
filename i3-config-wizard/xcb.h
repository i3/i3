#ifndef _XCB_H
#define _XCB_H

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK                    0xff7f

#define xmacro(atom) xcb_atom_t A_ ## atom;
#include "atoms.xmacro"
#undef xmacro

extern unsigned int xcb_numlock_mask;

xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height);
int get_font_id(xcb_connection_t *conn, char *pattern, int *font_height);

#endif
