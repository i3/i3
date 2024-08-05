/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * tiling_drag.h: Reposition tiled windows by dragging.
 *
 */
#pragma once

#include "all.h"

/**
 * Tiling drag initiation modes.
 */
typedef enum {
    TILING_DRAG_OFF = 0,
    TILING_DRAG_MODIFIER = 1,
    TILING_DRAG_TITLEBAR = 2,
    TILING_DRAG_MODIFIER_OR_TITLEBAR = 3
} tiling_drag_t;

/**
 * Returns whether there currently are any drop targets.
 * Used to only initiate a drag when there is something to drop onto.
 *
 */
bool has_drop_targets(void);

/**
 * Initiates a mouse drag operation on a tiled window.
 *
 */
void tiling_drag(Con *con, xcb_button_press_event_t *event, bool use_threshold);
