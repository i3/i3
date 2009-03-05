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
#include "i3.h"

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
                workspaces[i].num = i;
                SLIST_INIT(&(workspaces[i].dock_clients));
                expand_table_cols(&(workspaces[i]));
                expand_table_rows(&(workspaces[i]));
        }
}

static void new_container(Workspace *workspace, Container **container, int col, int row) {
        Container *new;
        new = *container = calloc(sizeof(Container), 1);
        CIRCLEQ_INIT(&(new->clients));
        new->colspan = 1;
        new->rowspan = 1;
        new->col = col;
        new->row = row;
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
                new_container(workspace, &(workspace->table[c][workspace->rows-1]), c, workspace->rows-1);
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
                new_container(workspace, &(workspace->table[workspace->cols-1][c]), workspace->cols-1, c);
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

static void move_columns_from(xcb_connection_t *conn, Workspace *workspace, int cols) {
        printf("firstly freeing \n");

        /* Clean up the column to be freed */
        for (int rows = 0; rows < workspace->rows; rows++) {
                Container *old_container = workspace->table[cols-1][rows];

                if (old_container->mode == MODE_STACK)
                        leave_stack_mode(conn, old_container);

                free(old_container);
        }

        for (; cols < workspace->cols; cols++)
                for (int rows = 0; rows < workspace->rows; rows++) {
                        printf("at col = %d, row = %d\n", cols, rows);
                        Container *new_container = workspace->table[cols][rows];

                        printf("moving cols = %d to cols -1 = %d\n", cols, cols-1);
                        workspace->table[cols-1][rows] = new_container;

                        new_container->row = rows;
                        new_container->col = cols-1;
                }
}

static void move_rows_from(xcb_connection_t *conn, Workspace *workspace, int rows) {
        for (int cols = 0; cols < workspace->cols; cols++) {
                Container *old_container = workspace->table[cols][rows-1];

                if (old_container->mode == MODE_STACK)
                        leave_stack_mode(conn, old_container);

                free(old_container);
        }
        for (; rows < workspace->rows; rows++)
                for (int cols = 0; cols < workspace->cols; cols++) {
                        Container *new_container = workspace->table[cols][rows];

                        printf("moving rows = %d to rows -1 = %d\n", rows, rows - 1);
                        workspace->table[cols][rows-1] = new_container;

                        new_container->row = rows-1;
                        new_container->col = cols;
                }
}

void dump_table(xcb_connection_t *conn, Workspace *workspace) {
        printf("dump_table()\n");
        for (int cols = 0; cols < workspace->cols; cols++) {
                for (int rows = 0; rows < workspace->rows; rows++) {
                        Container *con = workspace->table[cols][rows];
                        printf("----\n");
                        printf("at col=%d, row=%d\n", cols, rows);
                        printf("currently_focused = %p\n", con->currently_focused);
                        Client *loop;
                        CIRCLEQ_FOREACH(loop, &(con->clients), clients) {
                                printf("got client %08x / %s\n", loop->child, loop->name);
                        }
                        printf("----\n");
                }
        }
        printf("done\n");
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
                                move_columns_from(conn, workspace, cols+1);
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
                                move_rows_from(conn, workspace, rows+1);
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

/*
 * Fixes col/rowspan (makes sure there are no overlapping windows)
 *
 */
void fix_colrowspan(xcb_connection_t *conn, Workspace *workspace) {
        printf("Fixing col/rowspan\n");

        for (int cols = 0; cols < workspace->cols; cols++)
                for (int rows = 0; rows < workspace->rows; rows++) {
                        Container *con = workspace->table[cols][rows];
                        if (con->colspan > 1) {
                                printf("gots one with colspan %d\n", con->colspan);
                                while (con->colspan > 1 &&
                                       workspace->table[cols + (con->colspan - 1)][rows]->currently_focused != NULL)
                                        con->colspan--;
                                printf("fixed it to %d\n", con->colspan);
                        }
                        if (con->rowspan > 1) {
                                printf("gots one with rowspan %d\n", con->rowspan);
                                while (con->rowspan > 1 &&
                                       workspace->table[cols][rows + (con->rowspan - 1)]->currently_focused != NULL)
                                        con->rowspan--;
                                printf("fixed it to %d\n", con->rowspan);
                        }
                }
}
