/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include "data.h"

#ifndef _XINERAMA_H
#define _XINERAMA_H

TAILQ_HEAD(screens_head, Screen);
extern struct screens_head *virtual_screens;

/**
 * Returns true if both screen objects describe the same screen (checks their
 * size and position).
 *
 */
bool screens_are_equal(i3Screen *screen1, i3Screen *screen2);

/**
 * We have just established a connection to the X server and need the initial
 * Xinerama information to setup workspaces for each screen.
 *
 */
void initialize_xinerama(xcb_connection_t *conn);

/**
 * This is called when the rootwindow receives a configure_notify event and
 * therefore the number/position of the Xinerama screens could have changed.
 *
 */
void xinerama_requery_screens(xcb_connection_t *conn);

/**
 * Looks in virtual_screens for the i3Screen whose start coordinates are x, y
 *
 */
i3Screen *get_screen_at(int x, int y, struct screens_head *screenlist);

/**
 * Looks in virtual_screens for the i3Screen which contains coordinates x, y
 *
 */
i3Screen *get_screen_containing(int x, int y);

/**
 * Gets the screen which is the last one in the given direction, for example
 * the screen on the most bottom when direction == D_DOWN, the screen most
 * right when direction == D_RIGHT and so on.
 *
 * This function always returns a screen.
 *
 */
i3Screen *get_screen_most(direction_t direction);

#endif
