/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * resize.c: Interactive resizing.
 *
 */
#pragma once

#include <config.h>

bool resize_find_tiling_participants(Con **current, Con **other, direction_t direction, bool both_sides);

int resize_graphical_handler(Con *first, Con *second, orientation_t orientation, const xcb_button_press_event_t *event);
