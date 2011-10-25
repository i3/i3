/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * For more information on RandR, please see the X.org RandR specification at
 * http://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
 * (take your time to read it completely, it answers all questions).
 *
 */
#ifndef _RANDR_H
#define _RANDR_H

#include "data.h"
#include <xcb/randr.h>

TAILQ_HEAD(outputs_head, xoutput);
extern struct outputs_head outputs;

/**
 * We have just established a connection to the X server and need the initial
 * XRandR information to setup workspaces for each screen.
 *
 */
void randr_init(int *event_base);

/**
 * Disables RandR support by creating exactly one output with the size of the
 * X11 screen.
 *
 */
void disable_randr(xcb_connection_t *conn);

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
void init_ws_for_output(Output *output, Con *content);

/**
 * Initializes the specified output, assigning the specified workspace to it.
 *
 */
//void initialize_output(xcb_connection_t *conn, Output *output, Workspace *workspace);

/**
 * (Re-)queries the outputs via RandR and stores them in the list of outputs.
 *
 */
void randr_query_outputs();

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

/**
 * Gets the output which is the next one in the given direction.
 *
 */
Output *get_output_next(direction_t direction, Output *current);

#endif
