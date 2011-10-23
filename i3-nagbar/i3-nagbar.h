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

xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height);

#endif
