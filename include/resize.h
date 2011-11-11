/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * resize.c: Interactive resizing.
 *
 */
#ifndef _RESIZE_H
#define _RESIZE_H

int resize_graphical_handler(Con *first, Con *second, orientation_t orientation, const xcb_button_press_event_t *event);

#endif
