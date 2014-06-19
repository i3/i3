#pragma once

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK 0xff7f

#define xmacro(atom) xcb_atom_t A_##atom;
#include "atoms.xmacro"
#undef xmacro
