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

void init_table();
void expand_table_rows(Workspace *workspace);
void expand_table_cols(Workspace *workspace);
bool cell_exists(int col, int row);
void cleanup_table(xcb_connection_t *conn, Workspace *workspace);

#endif
