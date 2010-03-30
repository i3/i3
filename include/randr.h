/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include "data.h"
#include <xcb/randr.h>

#ifndef _RANDR_H
#define _RANDR_H

TAILQ_HEAD(outputs_head, xoutput);
extern struct outputs_head outputs;

/**
 * We have just established a connection to the X server and need the initial
 * XRandR information to setup workspaces for each screen.
 *
 */
void initialize_randr(xcb_connection_t *conn, int *event_base);

/**
 * Disables RandR support by creating exactly one output with the size of the
 * X11 screen.
 *
 */
void disable_randr(xcb_connection_t *conn);

/**
 * Initializes the specified output, assigning the specified workspace to it.
 *
 */
void initialize_output(xcb_connection_t *conn, Output *output, Workspace *workspace);

/**
 * (Re-)queries the outputs via RandR and stores them in the list of outputs.
 *
 */
void randr_query_outputs(xcb_connection_t *conn);

/**
 * Returns the first output which is active.
 *
 */
Output *get_first_output();

/**
 * Returns the output with the given name if it is active (!) or NULL.
 *
 */
Output *get_output_by_name(const char *name);

/**
 * Returns the active (!) output which contains the coordinates x, y or NULL
 * if there is no output which contains these coordinates.
 *
 */
Output *get_output_containing(int x, int y);

/**
 * Gets the output which is the last one in the given direction, for example
 * the output on the most bottom when direction == D_DOWN, the output most
 * right when direction == D_RIGHT and so on.
 *
 * This function always returns a output.
 *
 */
Output *get_output_most(direction_t direction, Output *current);

#endif
