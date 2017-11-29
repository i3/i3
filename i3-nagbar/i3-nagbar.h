#pragma once

#include <config.h>

#include <err.h>

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define FREE(pointer)   \
    do {                \
        free(pointer);  \
        pointer = NULL; \
    } while (0)

#define xmacro(atom) xcb_atom_t A_##atom;
#include "atoms.xmacro"
#undef xmacro

extern xcb_window_t root;
