/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * click.c: Button press (mouse click) events.
 *
 */
#pragma once

/**
 * The button press X callback. This function determines whether the floating
 * modifier is pressed and where the user clicked (decoration, border, inside
 * the window).
 *
 * Then, route_click is called on the appropriate con.
 *
 */
int handle_button_press(xcb_button_press_event_t *event);
