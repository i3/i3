/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _CLICK_H
#define _CLICK_H

/**
 * The button press X callback. This function determines whether the floating
 * modifier is pressed and where the user clicked (decoration, border, inside
 * the window).
 *
 * Then, route_click is called on the appropriate con.
 *
 */
int handle_button_press(xcb_button_press_event_t *event);

#endif
