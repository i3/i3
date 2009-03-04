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
#include <xcb/xcb.h>

#ifndef _LAYOUT_H
#define _LAYOUT_H

Rect get_unoccupied_space(Workspace *workspace);
void decorate_window(xcb_connection_t *conn, Client *client, xcb_drawable_t drawable, xcb_gcontext_t gc, int offset);
void redecorate_window(xcb_connection_t *conn, Client *client);
void render_container(xcb_connection_t *conn, Container *container);
void render_layout(xcb_connection_t *conn);

#endif
