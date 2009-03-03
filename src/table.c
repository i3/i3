/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * table.c: Functions/macros for easy modifying/accessing of _the_ table (defining our
 *          layout).
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
#include "util.h"

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
                SLIST_INIT(&(workspaces[i].dock_clients));
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

static void shrink_table_cols(Workspace *workspace) {
        workspace->cols--;
        free(workspace->table[workspace->cols]);
        workspace->table = realloc(workspace->table, sizeof(Container**) * workspace->cols);
}

static void shrink_table_rows(Workspace *workspace) {
        workspace->rows--;
        for (int cols = 0; cols < workspace->cols; cols++)
                workspace->table[cols] = realloc(workspace->table[cols], sizeof(Container*) * workspace->rows);
}


/*
 * Performs simple bounds checking for the given column/row
 *
 */
bool cell_exists(int col, int row) {
        return (col >= 0 && col < c_ws->cols) &&
               (row >= 0 && row < c_ws->rows);
}

static void move_columns_from(Workspace *workspace, int cols) {
        for (; cols < workspace->cols; cols++)
                for (int rows = 0; rows < workspace->rows; rows++) {
                        free(workspace->table[cols-1][rows]);

                        printf("moving cols = %d to cols -1 = %d\n", cols, cols-1);
                        workspace->table[cols-1][rows] = workspace->table[cols][rows];
                        workspace->table[cols-1][rows]->col--;
                        workspace->table[cols][rows] = NULL;
                }
}

static void move_rows_from(Workspace *workspace, int rows) {
        for (; rows < workspace->rows; rows++)
                for (int cols = 0; cols < workspace->cols; cols++) {
                        free(workspace->table[cols][rows-1]);

                        printf("moving rows = %d to rows -1 = %d\n", rows, rows - 1);
                        workspace->table[cols][rows-1] = workspace->table[cols][rows];
                        workspace->table[cols][rows-1]->row--;
                        workspace->table[cols][rows] = NULL;
                }
}

/*
 * Shrinks the table by "compacting" it, that is, removing completely empty rows/columns
 *
 */
void cleanup_table(xcb_connection_t *conn, Workspace *workspace) {
        printf("cleanup_table()\n");

        /* Check for empty columns if we got more than one column */
        for (int cols = 0; (workspace->cols > 1) && (cols < workspace->cols);) {
                bool completely_empty = true;
                for (int rows = 0; rows < workspace->rows; rows++)
                        if (workspace->table[cols][rows]->currently_focused != NULL) {
                                completely_empty = false;
                                break;
                        }
                if (completely_empty) {
                        printf("Removing completely empty column %d\n", cols);
                        if (cols < (workspace->cols - 1))
                                move_columns_from(workspace, cols+1);
                        shrink_table_cols(workspace);

                        if (workspace->current_col >= workspace->cols)
                                workspace->current_col = workspace->cols - 1;
                } else cols++;
        }

        /* Check for empty rows if we got more than one row*/
        for (int rows = 0; (workspace->rows > 1) && (rows < workspace->rows);) {
                bool completely_empty = true;
                for (int cols = 0; cols < workspace->cols; cols++)
                        if (workspace->table[cols][rows]->currently_focused != NULL) {
                                completely_empty = false;
                                break;
                        }
                if (completely_empty) {
                        printf("Removing completely empty row %d\n", rows);
                        if (rows < (workspace->rows - 1))
                                move_rows_from(workspace, rows+1);
                        shrink_table_rows(workspace);

                        if (workspace->current_row >= workspace->rows)
                                workspace->current_row = workspace->rows - 1;
                } else rows++;
        }

        /* Boundary checking for current_col and current_row */
        if (current_col >= c_ws->cols)
                current_col = c_ws->cols-1;

        if (current_row >= c_ws->rows)
                current_row = c_ws->rows-1;

        if (CUR_CELL->currently_focused != NULL)
                set_focus(conn, CUR_CELL->currently_focused);
}
