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
#include "layout.h"
#include "config.h"
#include "workspace.h"
#include "log.h"

int current_workspace = 0;
int num_workspaces = 1;
struct workspaces_head *workspaces;
/* Convenience pointer to the current workspace */
Workspace *c_ws;
int current_col = 0;
int current_row = 0;

/*
 * Initialize table
 *
 */
void init_table() {
        workspaces = scalloc(sizeof(struct workspaces_head));
        TAILQ_INIT(workspaces);

        c_ws = scalloc(sizeof(Workspace));
        workspace_set_name(c_ws, NULL);
        TAILQ_INIT(&(c_ws->floating_clients));
        TAILQ_INSERT_TAIL(workspaces, c_ws, workspaces);
}

static void new_container(Workspace *workspace, Container **container, int col, int row, bool skip_layout_switch) {
        Container *new;
        new = *container = calloc(sizeof(Container), 1);
        CIRCLEQ_INIT(&(new->clients));
        new->colspan = 1;
        new->rowspan = 1;
        new->col = col;
        new->row = row;
        new->workspace = workspace;
        if (!skip_layout_switch)
                switch_layout_mode(global_conn, new, config.container_mode);
        new->stack_limit = config.container_stack_limit;
        new->stack_limit_value = config.container_stack_limit_value;
}

/*
 * Add one row to the table
 *
 */
void expand_table_rows(Workspace *workspace) {
        workspace->rows++;

        workspace->height_factor = realloc(workspace->height_factor, sizeof(float) * workspace->rows);
        workspace->height_factor[workspace->rows-1] = 0;

        for (int c = 0; c < workspace->cols; c++) {
                workspace->table[c] = realloc(workspace->table[c], sizeof(Container*) * workspace->rows);
                new_container(workspace, &(workspace->table[c][workspace->rows-1]), c, workspace->rows-1, true);
        }

        /* We need to switch the layout in a separate step because it could
         * happen that render_layout() (being called by switch_layout_mode())
         * would access containers which were not yet initialized. */
        for (int c = 0; c < workspace->cols; c++)
                switch_layout_mode(global_conn, workspace->table[c][workspace->rows-1], config.container_mode);
}

/*
 * Adds one row at the head of the table
 *
 */
void expand_table_rows_at_head(Workspace *workspace) {
        workspace->rows++;

        workspace->height_factor = realloc(workspace->height_factor, sizeof(float) * workspace->rows);

        DLOG("rows = %d\n", workspace->rows);
        for (int rows = (workspace->rows - 1); rows >= 1; rows--) {
                DLOG("Moving height_factor %d (%f) to %d\n", rows-1, workspace->height_factor[rows-1], rows);
                workspace->height_factor[rows] = workspace->height_factor[rows-1];
        }

        workspace->height_factor[0] = 0;

        for (int cols = 0; cols < workspace->cols; cols++)
                workspace->table[cols] = realloc(workspace->table[cols], sizeof(Container*) * workspace->rows);

        /* Move the other rows */
        for (int cols = 0; cols < workspace->cols; cols++)
                for (int rows = workspace->rows - 1; rows > 0; rows--) {
                        DLOG("Moving row %d to %d\n", rows-1, rows);
                        workspace->table[cols][rows] = workspace->table[cols][rows-1];
                        workspace->table[cols][rows]->row = rows;
                }

        for (int cols = 0; cols < workspace->cols; cols++)
                new_container(workspace, &(workspace->table[cols][0]), cols, 0, false);
}

/*
 * Add one column to the table
 *
 */
void expand_table_cols(Workspace *workspace) {
        workspace->cols++;

        workspace->width_factor = realloc(workspace->width_factor, sizeof(float) * workspace->cols);
        workspace->width_factor[workspace->cols-1] = 0;

        workspace->table = realloc(workspace->table, sizeof(Container**) * workspace->cols);
        workspace->table[workspace->cols-1] = calloc(sizeof(Container*) * workspace->rows, 1);

        for (int c = 0; c < workspace->rows; c++)
                new_container(workspace, &(workspace->table[workspace->cols-1][c]), workspace->cols-1, c, true);

        for (int c = 0; c < workspace->rows; c++)
                switch_layout_mode(global_conn, workspace->table[workspace->cols-1][c], config.container_mode);
}

/*
 * Inserts one column at the table’s head
 *
 */
void expand_table_cols_at_head(Workspace *workspace) {
        workspace->cols++;

        workspace->width_factor = realloc(workspace->width_factor, sizeof(float) * workspace->cols);

        DLOG("cols = %d\n", workspace->cols);
        for (int cols = (workspace->cols - 1); cols >= 1; cols--) {
                DLOG("Moving width_factor %d (%f) to %d\n", cols-1, workspace->width_factor[cols-1], cols);
                workspace->width_factor[cols] = workspace->width_factor[cols-1];
        }

        workspace->width_factor[0] = 0;

        workspace->table = realloc(workspace->table, sizeof(Container**) * workspace->cols);
        workspace->table[workspace->cols-1] = calloc(sizeof(Container*) * workspace->rows, 1);

        /* Move the other columns */
        for (int rows = 0; rows < workspace->rows; rows++)
                for (int cols = workspace->cols - 1; cols > 0; cols--) {
                        DLOG("Moving col %d to %d\n", cols-1, cols);
                        workspace->table[cols][rows] = workspace->table[cols-1][rows];
                        workspace->table[cols][rows]->col = cols;
                }

        for (int rows = 0; rows < workspace->rows; rows++)
                new_container(workspace, &(workspace->table[0][rows]), 0, rows, false);
}

/*
 * Shrinks the table by one column.
 *
 * The containers themselves are freed in move_columns_from() or move_rows_from(). Therefore, this
 * function may only be called from move_*() or after making sure that the containers are freed
 * properly.
 *
 */
static void shrink_table_cols(Workspace *workspace) {
        float free_space = workspace->width_factor[workspace->cols-1];

        workspace->cols--;

        /* Shrink the width_factor array */
        workspace->width_factor = realloc(workspace->width_factor, sizeof(float) * workspace->cols);

        /* Free the container-pointers */
        free(workspace->table[workspace->cols]);

        /* Re-allocate the table */
        workspace->table = realloc(workspace->table, sizeof(Container**) * workspace->cols);

        /* Distribute the free space */
        if (free_space == 0)
                return;

        for (int cols = (workspace->cols-1); cols >= 0; cols--) {
                if (workspace->width_factor[cols] == 0)
                        continue;

                DLOG("Added free space (%f) to %d (had %f)\n", free_space, cols,
                                workspace->width_factor[cols]);
                workspace->width_factor[cols] += free_space;
                break;
        }
}

/*
 * See shrink_table_cols()
 *
 */
static void shrink_table_rows(Workspace *workspace) {
        float free_space = workspace->height_factor[workspace->rows-1];

        workspace->rows--;
        for (int cols = 0; cols < workspace->cols; cols++)
                workspace->table[cols] = realloc(workspace->table[cols], sizeof(Container*) * workspace->rows);

        /* Shrink the height_factor array */
        workspace->height_factor = realloc(workspace->height_factor, sizeof(float) * workspace->rows);

        /* Distribute the free space */
        if (free_space == 0)
                return;

        for (int rows = (workspace->rows-1); rows >= 0; rows--) {
                if (workspace->height_factor[rows] == 0)
                        continue;

                DLOG("Added free space (%f) to %d (had %f)\n", free_space, rows,
                                workspace->height_factor[rows]);
                workspace->height_factor[rows] += free_space;
                break;
        }
}

/*
 * Performs simple bounds checking for the given column/row
 *
 */
bool cell_exists(Workspace *ws, int col, int row) {
        return (col >= 0 && col < ws->cols) &&
               (row >= 0 && row < ws->rows);
}

static void free_container(xcb_connection_t *conn, Workspace *workspace, int col, int row) {
        Container *old_container = workspace->table[col][row];

        if (old_container->mode == MODE_STACK || old_container->mode == MODE_TABBED)
                leave_stack_mode(conn, old_container);

        free(old_container);
}

static void move_columns_from(xcb_connection_t *conn, Workspace *workspace, int cols) {
        DLOG("firstly freeing \n");

        /* Free the columns which are cleaned up */
        for (int rows = 0; rows < workspace->rows; rows++)
                free_container(conn, workspace, cols-1, rows);

        for (; cols < workspace->cols; cols++)
                for (int rows = 0; rows < workspace->rows; rows++) {
                        DLOG("at col = %d, row = %d\n", cols, rows);
                        Container *new_container = workspace->table[cols][rows];

                        DLOG("moving cols = %d to cols -1 = %d\n", cols, cols-1);
                        workspace->table[cols-1][rows] = new_container;

                        new_container->row = rows;
                        new_container->col = cols-1;
                }
}

static void move_rows_from(xcb_connection_t *conn, Workspace *workspace, int rows) {
        for (int cols = 0; cols < workspace->cols; cols++)
                free_container(conn, workspace, cols, rows-1);

        for (; rows < workspace->rows; rows++)
                for (int cols = 0; cols < workspace->cols; cols++) {
                        Container *new_container = workspace->table[cols][rows];

                        DLOG("moving rows = %d to rows -1 = %d\n", rows, rows - 1);
                        workspace->table[cols][rows-1] = new_container;

                        new_container->row = rows-1;
                        new_container->col = cols;
                }
}

/*
 * Prints the table’s contents in human-readable form for debugging
 *
 */
void dump_table(xcb_connection_t *conn, Workspace *workspace) {
        DLOG("dump_table()\n");
        FOR_TABLE(workspace) {
                Container *con = workspace->table[cols][rows];
                DLOG("----\n");
                DLOG("at col=%d, row=%d\n", cols, rows);
                DLOG("currently_focused = %p\n", con->currently_focused);
                Client *loop;
                CIRCLEQ_FOREACH(loop, &(con->clients), clients) {
                        DLOG("got client %08x / %s\n", loop->child, loop->name);
                }
                DLOG("----\n");
        }
        DLOG("done\n");
}

/*
 * Shrinks the table by "compacting" it, that is, removing completely empty rows/columns
 *
 */
void cleanup_table(xcb_connection_t *conn, Workspace *workspace) {
        DLOG("cleanup_table()\n");

        /* Check for empty columns if we got more than one column */
        for (int cols = 0; (workspace->cols > 1) && (cols < workspace->cols);) {
                bool completely_empty = true;
                for (int rows = 0; rows < workspace->rows; rows++)
                        if (workspace->table[cols][rows]->currently_focused != NULL) {
                                completely_empty = false;
                                break;
                        }
                if (completely_empty) {
                        DLOG("Removing completely empty column %d\n", cols);
                        if (cols < (workspace->cols - 1))
                                move_columns_from(conn, workspace, cols+1);
                        else {
                                for (int rows = 0; rows < workspace->rows; rows++)
                                        free_container(conn, workspace, cols, rows);
                        }
                        shrink_table_cols(workspace);

                        if (workspace->current_col >= workspace->cols)
                                workspace->current_col = workspace->cols - 1;
                } else cols++;
        }

        /* Check for empty rows if we got more than one row */
        for (int rows = 0; (workspace->rows > 1) && (rows < workspace->rows);) {
                bool completely_empty = true;
                DLOG("Checking row %d\n", rows);
                for (int cols = 0; cols < workspace->cols; cols++)
                        if (workspace->table[cols][rows]->currently_focused != NULL) {
                                completely_empty = false;
                                break;
                        }
                if (completely_empty) {
                        DLOG("Removing completely empty row %d\n", rows);
                        if (rows < (workspace->rows - 1))
                                move_rows_from(conn, workspace, rows+1);
                        else {
                                for (int cols = 0; cols < workspace->cols; cols++)
                                        free_container(conn, workspace, cols, rows);
                        }
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
                set_focus(conn, CUR_CELL->currently_focused, true);
}

/*
 * Fixes col/rowspan (makes sure there are no overlapping windows, obeys borders).
 *
 */
void fix_colrowspan(xcb_connection_t *conn, Workspace *workspace) {
        DLOG("Fixing col/rowspan\n");

        FOR_TABLE(workspace) {
                Container *con = workspace->table[cols][rows];
                if (con->colspan > 1) {
                        DLOG("gots one with colspan %d (at %d c, %d r)\n", con->colspan, cols, rows);
                        while (con->colspan > 1 &&
                               (!cell_exists(workspace, cols + (con->colspan-1), rows) &&
                                workspace->table[cols + (con->colspan - 1)][rows]->currently_focused != NULL))
                                con->colspan--;
                        DLOG("fixed it to %d\n", con->colspan);
                }
                if (con->rowspan > 1) {
                        DLOG("gots one with rowspan %d (at %d c, %d r)\n", con->rowspan, cols, rows);
                        while (con->rowspan > 1 &&
                               (!cell_exists(workspace, cols, rows + (con->rowspan - 1)) &&
                                workspace->table[cols][rows + (con->rowspan - 1)]->currently_focused != NULL))
                                con->rowspan--;
                        DLOG("fixed it to %d\n", con->rowspan);
                }
        }
}
