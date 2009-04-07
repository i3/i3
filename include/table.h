/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdbool.h>

#include <xcb/xcb.h>

#include "data.h"

#ifndef _TABLE_H
#define _TABLE_H

#define CUR_TABLE (c_ws->table)
#define CUR_CELL (CUR_TABLE[current_col][current_row])

extern Workspace *c_ws;
extern Workspace workspaces[10];
extern int current_col;
extern int current_row;

/** Initialize table */
void init_table();

/** Add one row to the table */
void expand_table_rows(Workspace *workspace);

/** Adds one row at the head of the table */
void expand_table_rows_at_head(Workspace *workspace);

/** Add one column to the table */
void expand_table_cols(Workspace *workspace);

/** Inserts one column at the table’s head */
void expand_table_cols_at_head(Workspace *workspace);

/** Performs simple bounds checking for the given column/row */
bool cell_exists(int col, int row);

/** Shrinks the table by "compacting" it, that is, removing completely empty rows/columns */
void cleanup_table(xcb_connection_t *conn, Workspace *workspace);

/** Fixes col/rowspan (makes sure there are no overlapping windows) */
void fix_colrowspan(xcb_connection_t *conn, Workspace *workspace);

/** Prints the table’s contents in human-readable form for debugging */
void dump_table(xcb_connection_t *conn, Workspace *workspace);

#endif
