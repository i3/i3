/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _CLICK_H
#define _CLICK_H

int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event);

#endif
