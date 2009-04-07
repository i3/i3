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

/**
 * (Re-)draws window decorations for a given Client onto the given drawable/graphic context.
 * When in stacking mode, the window decorations are drawn onto an own window.
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client, xcb_drawable_t drawable, xcb_gcontext_t gc, int offset);

/**
 * Redecorates the given client correctly by checking if it’s in a stacking container and
 * re-rendering the stack window or just calling decorate_window if it’s not in a stacking
 * container.
 *
 */
void redecorate_window(xcb_connection_t *conn, Client *client);

/**
 * Renders the given container. Is called by render_layout() or individually (for example
 * when focus changes in a stacking container)
 *
 */
void render_container(xcb_connection_t *conn, Container *container);

/**
 * Modifies the event mask of all clients on the given workspace to either ignore or to handle
 * enter notifies. It is handy to ignore notifies because they will be sent when a window is mapped
 * under the cursor, thus when the user didn’t enter the window actively at all.
 *
 */
void ignore_enter_notify_forall(xcb_connection_t *conn, Workspace *workspace, bool ignore_enter_notify);

/**
 * Renders the whole layout, that is: Go through each screen, each workspace, each container
 * and render each client. This also renders the bars.
 *
 * If you don’t need to render *everything*, you should call render_container on the container
 * you want to refresh.
 *
 */
void render_layout(xcb_connection_t *conn);

#endif
