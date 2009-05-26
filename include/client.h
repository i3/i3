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

#include "data.h"

#ifndef _CLIENT_H
#define _CLIENT_H

/**
 * Removes the given client from the container, either because it will be inserted into another
 * one or because it was unmapped
 *
 */
void client_remove_from_container(xcb_connection_t *conn, Client *client, Container *container);

/**
 * Warps the pointer into the given client (in the middle of it, to be specific), therefore
 * selecting it
 *
 */
void client_warp_pointer_into(xcb_connection_t *conn, Client *client);

/**
 * Kills the given window using WM_DELETE_WINDOW or xcb_kill_window
 *
 */
void client_kill(xcb_connection_t *conn, Client *window);

/**
 * Checks if the given window class and title match the given client
 * Window title is passed as "normal" string and as UCS-2 converted string for
 * matching _NET_WM_NAME capable clients as well as those using legacy hints.
 *
 */
bool client_matches_class_name(Client *client, char *to_class, char *to_title,
                               char *to_title_ucs, int to_title_ucs_len);

/**
 * Toggles fullscreen mode for the given client. It updates the data structures and
 * reconfigures (= resizes/moves) the client and its frame to the full size of the
 * screen. When leaving fullscreen, re-rendering the layout is forced.
 *
 */
void client_toggle_fullscreen(xcb_connection_t *conn, Client *client);

#endif
