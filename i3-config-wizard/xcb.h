#pragma once

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK 0xff7f

#include "i3-config-wizard-atoms.xmacro.h"

#define xmacro(atom) xcb_atom_t A_##atom;
CONFIG_WIZARD_ATOMS_XMACRO
#undef xmacro
