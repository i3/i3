/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * For more information on RandR, please see the X.org RandR specification at
 * https://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
 * (take your time to read it completely, it answers all questions).
 *
 */
#pragma once

#include <config.h>

#include "data.h"
#include <xcb/randr.h>

TAILQ_HEAD(outputs_head, xoutput);
extern struct outputs_head outputs;

typedef enum {
    CLOSEST_OUTPUT = 0,
    FARTHEST_OUTPUT = 1
} output_close_far_t;

/**
 * We have just established a connection to the X server and need the initial
 * XRandR information to setup workspaces for each screen.
 *
 */
void randr_init(int *event_base, const bool disable_randr15);

/**
 * Initializes a CT_OUTPUT Con (searches existing ones from inplace restart
 * before) to use for the given Output.
 *
 */
void output_init_con(Output *output);

/**
 * Initializes at least one workspace for this output, trying the following
 * steps until there is at least one workspace:
 *
 * • Move existing workspaces, which are assigned to be on the given output, to
 *   the output.
 * • Create the first assigned workspace for this output.
 * • Create the first unused workspace.
 *
 */
void init_ws_for_output(Output *output);

/**
 * (Re-)queries the outputs via RandR and stores them in the list of outputs.
 *
 */
void randr_query_outputs(void);

/**
 * Disables the output and moves its content.
 *
 */
void randr_disable_output(Output *output);

/**
 * Returns the first output which is active.
 *
 */
Output *get_first_output(void);

/**
 * Returns the output with the given name or NULL.
 * If require_active is true, only active outputs are considered.
 *
 */
Output *get_output_by_name(const char *name, const bool require_active);

/**
 * Returns the active (!) output which contains the coordinates x, y or NULL
 * if there is no output which contains these coordinates.
 *
 */
Output *get_output_containing(unsigned int x, unsigned int y);

/**
 * Returns the active output which contains the midpoint of the given rect. If
 * such an output doesn't exist, returns the output which contains most of the
 * rectangle or NULL if there is no output which intersects with it.
 *
 */
Output *get_output_from_rect(Rect rect);

/**
 * Returns the active output which spans exactly the area specified by
 * rect or NULL if there is no output like this.
 *
 */
Output *get_output_with_dimensions(Rect rect);

/**
 * In output_containing_rect, we check if any active output contains part of the container.
 * We do this by checking if the output rect is intersected by the Rect.
 * This is the 2-dimensional counterpart of get_output_containing.
 * Returns the output with the maximum intersecting area.
 *
 */
Output *output_containing_rect(Rect rect);

/**
 * Gets the output which is the next one in the given direction.
 *
 * If close_far == CLOSEST_OUTPUT, then the output next to the current one will
 * selected.  If close_far == FARTHEST_OUTPUT, the output which is the last one
 * in the given direction will be selected.
 *
 * NULL will be returned when no active outputs are present in the direction
 * specified (note that ‘current’ counts as such an output).
 *
 */
Output *get_output_next(direction_t direction, Output *current, output_close_far_t close_far);

/**
 * Like get_output_next with close_far == CLOSEST_OUTPUT, but wraps.
 *
 * For example if get_output_next(D_DOWN, x, FARTHEST_OUTPUT) = NULL, then
 * get_output_next_wrap(D_DOWN, x) will return the topmost output.
 *
 * This function always returns a output: if no active outputs can be found,
 * current itself is returned.
 *
 */
Output *get_output_next_wrap(direction_t direction, Output *current);

/**
 * Creates an output covering the root window.
 *
 */
Output *create_root_output(xcb_connection_t *conn);
