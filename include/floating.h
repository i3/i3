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
#ifndef _FLOATING_H
#define _FLOATING_H

/**
 * Enters floating mode for the given client.
 * Correctly takes care of the position/size (separately stored for tiling/floating mode)
 * and repositions/resizes/redecorates the client.
 *
 */
void toggle_floating_mode(xcb_connection_t *conn, Client *client);

/**
 * Called whenever the user clicks on a border (not the titlebar!) of a floating window.
 * Determines on which border the user clicked and launches the drag_pointer function
 * with the resize_callback.
 *
 */
int floating_border_click(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event);

/**
 * Called when the user clicked on the titlebar of a floating window.
 * Calls the drag_pointer function with the drag_window callback
 *
 */
void floating_drag_window(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event);

#endif
