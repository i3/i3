/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#pragma once

#include <stdbool.h>
#include "data.h"

/**
 * Calculates the effective gap sizes for a container.
 */
gaps_t calculate_effective_gaps(Con *con);

/*
 * Decides whether the container should be inset.
 */
bool gaps_should_inset_con(Con *con, int children);

/*
 * Returns whether the given container has an adjacent container in the
 * specified direction. In other words, this returns true if and only if
 * the container is not touching the edge of the screen in that direction.
 */
bool gaps_has_adjacent_container(Con *con, direction_t direction);
