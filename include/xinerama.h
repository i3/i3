/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * This is LEGACY code (we support RandR, which can do much more than
 * Xinerama), but necessary for the poor users of the nVidia binary
 * driver which does not support RandR in 2011 *sigh*.
 *
 */
#pragma once

#include "data.h"

/**
 * We have just established a connection to the X server and need the initial
 * Xinerama information to setup workspaces for each screen.
 *
 */
void xinerama_init(void);
