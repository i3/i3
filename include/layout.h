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
 * Gets the unoccupied space (= space which is available for windows which
 * were resized by the user) This is necessary to render both, customly
 * resized windows and never touched windows correctly, meaning that the
 * aspect ratio will be maintained when opening new windows.
 *
 */
int get_unoccupied_x(Workspace *workspace);

/**
 * (Re-)draws window decorations for a given Client onto the given
 * drawable/graphic context.  When in stacking mode, the window decorations
 * are drawn onto an own window.
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client,
                     xcb_drawable_t drawable, xcb_gcontext_t gc, int offset);

/**
 * Redecorates the given client correctly by checking if it’s in a stacking
 * container and re-rendering the stack window or just calling decorate_window
 * if it’s not in a stacking container.
 *
 */
void redecorate_window(xcb_connection_t *conn, Client *client);

/**
 * Pushes the client’s x and y coordinates to X11
 *
 */
void reposition_client(xcb_connection_t *conn, Client *client);

/**
 * Pushes the client’s width/height to X11 and resizes the child window. This
 * function also updates the client’s position, so if you work on tiling clients
 * only, you can use this function instead of separate calls to reposition_client
 * and resize_client to reduce flickering.
 *
 */
void resize_client(xcb_connection_t *conn, Client *client);

/**
 * Renders the given container. Is called by render_layout() or individually
 * (for example when focus changes in a stacking container)
 *
 */
void render_container(xcb_connection_t *conn, Container *container);

/**
 * Modifies the event mask of all clients on the given workspace to either
 * ignore or to handle enter notifies. It is handy to ignore notifies because
 * they will be sent when a window is mapped under the cursor, thus when the
 * user didn’t enter the window actively at all.
 *
 */
void ignore_enter_notify_forall(xcb_connection_t *conn, Workspace *workspace,
                                bool ignore_enter_notify);

/**
 * Renders the given workspace on the given screen
 *
 */
void render_workspace(xcb_connection_t *conn, i3Screen *screen, Workspace *r_ws);

/**
 * Renders the whole layout, that is: Go through each screen, each workspace,
 * each container and render each client. This also renders the bars.
 *
 * If you don’t need to render *everything*, you should call render_container
 * on the container you want to refresh.
 *
 */
void render_layout(xcb_connection_t *conn);

#endif
