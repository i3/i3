/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include "data.h"

#ifndef _XINERAMA_H
#define _XINERAMA_H

/**
 * We have just established a connection to the X server and need the initial
 * Xinerama information to setup workspaces for each screen.
 *
 */
void initialize_xinerama(xcb_connection_t *conn);

#endif
