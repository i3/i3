#ifndef _XCB_H
#define _XCB_H

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK                    0xff7f

#define xmacro(atom) xcb_atom_t A_ ## atom;
#include "atoms.xmacro"
#undef xmacro

extern unsigned int xcb_numlock_mask;

void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value);
uint32_t get_colorpixel(xcb_connection_t *conn, char *hex);
uint32_t get_mod_mask(xcb_connection_t *conn, uint32_t keycode);
xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height);
int get_font_id(xcb_connection_t *conn, char *pattern, int *font_height);
/**
 * Finds out which modifier mask is the one for numlock, as the user may change this.
 *
 */
void xcb_get_numlock_mask(xcb_connection_t *conn);

#endif
