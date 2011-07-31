#ifndef _I3_NAGBAR
#define _I3_NAGBAR

#include <err.h>

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define FREE(pointer) do { \
        if (pointer != NULL) { \
                free(pointer); \
                pointer = NULL; \
        } \
} \
while (0)

#define xmacro(atom) xcb_atom_t A_ ## atom;
#include "atoms.xmacro"
#undef xmacro

extern xcb_window_t root;

uint32_t get_colorpixel(xcb_connection_t *conn, char *hex);
xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height);
int get_font_id(xcb_connection_t *conn, char *pattern, int *font_height);
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value);

#endif
