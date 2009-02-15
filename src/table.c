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
/*
 * This file provides functions for easier accessing of _the_ table
 *
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include "data.h"
#include "table.h"

int current_workspace = 0;
Workspace workspaces[10];
/* Convenience pointer to the current workspace */
Workspace *c_ws = &workspaces[0];
int current_col = 0;
int current_row = 0;

/*
 * Initialize table
 *
 */
void init_table() {
        memset(workspaces, 0, sizeof(workspaces));

        for (int i = 0; i < 10; i++) {
                workspaces[i].screen = NULL;
                expand_table_cols(&(workspaces[i]));
                expand_table_rows(&(workspaces[i]));
        }
}

static void new_container(Workspace *workspace, Container **container) {
        Container *new;
        new = *container = calloc(sizeof(Container), 1);
        CIRCLEQ_INIT(&(new->clients));
        new->colspan = 1;
        new->rowspan = 1;
        new->workspace = workspace;
}

/*
 * Add one row to the table
 *
 */
void expand_table_rows(Workspace *workspace) {
        workspace->rows++;

        for (int c = 0; c < workspace->cols; c++) {
                workspace->table[c] = realloc(workspace->table[c], sizeof(Container*) * workspace->rows);
                new_container(workspace, &(workspace->table[c][workspace->rows-1]));
        }
}

/*
 * Add one column to the table
 *
 */
void expand_table_cols(Workspace *workspace) {
        workspace->cols++;

        workspace->table = realloc(workspace->table, sizeof(Container**) * workspace->cols);
        workspace->table[workspace->cols-1] = calloc(sizeof(Container*) * workspace->rows, 1);
        for (int c = 0; c < workspace->rows; c++)
                new_container(workspace, &(workspace->table[workspace->cols-1][c]));
}

/*
 * Performs simple bounds checking for the given column/row
 *
 */
bool cell_exists(int col, int row) {
        return (col >= 0 && col < c_ws->rows) &&
                (row >= 0 && row < c_ws->cols);
}
