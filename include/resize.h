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

typedef enum { CLICK_BORDER = 0,
               CLICK_DECORATION = 1,
               CLICK_INSIDE = 2 } click_destination_t;

bool resize_find_tiling_participants(Con **current, Con **other, direction_t direction, bool both_sides);

void resize_graphical_handler(const xcb_button_press_event_t *event,
                              enum xcursor_cursor_t cursor,
                              bool use_threshold,
                              Con *output,
                              Con *first_h, Con *first_v, Con *second_h, Con *second_v);

/**
 * Resize the two given containers using the given amount of pixels or
 * percentage points. One of the two needs to be 0. A positive amount means
 * growing the first container while a negative means shrinking it.
 * Return false when resize is not performed due to container minimum size
 * constraints.
 *
 */
bool resize_neighboring_cons(Con *first, Con *second, int px, int ppt);

/**
 * Calculate the minimum percent needed for the given container to be at least 1
 * pixel.
 *
 */
double percent_for_1px(Con *con);

enum xcursor_cursor_t resize_cursor(border_t corner, bool both);

border_t find_corresponding_resize_borders(int x, int y, int width, int height);

/**
 * Return a bitmask of a corresponding resize borders for title/border drag.
 *
 */
border_t resize_get_borders_sides(Con *con, int x, int y, click_destination_t dest);

/**
 * Return a bitmask of a corresponding resize borders for mod+rightclick resize.
 *
 */
border_t resize_get_borders_mod(int x, int y, int width, int height);
