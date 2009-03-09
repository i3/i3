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

void initialize_xinerama(xcb_connection_t *conn);
void xinerama_requery_screens(xcb_connection_t *conn);
i3Screen *get_screen_at(int x, int y, struct screens_head *screenlist);
i3Screen *get_screen_containing(int x, int y);
i3Screen *get_screen_most(direction_t direction);

#endif
