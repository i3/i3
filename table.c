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

/*
 * Initialize table
 *
 */
void init_table() {
	int i;
	printf("sizof(workspaces) = %d\n", sizeof(workspaces));
	memset(workspaces, 0, sizeof(workspaces));

	for (i = 0; i < 10; i++) {
		expand_table_cols(&(workspaces[i]));
		expand_table_rows(&(workspaces[i]));
	}
}

static void new_container(Container **container) {
	Container *new;
	new = *container = calloc(sizeof(Container), 1);
	CIRCLEQ_INIT(&(new->clients));
	new->colspan = 1;
	new->rowspan = 1;
}

/*
 * Add one row to the table
 *
 */
void expand_table_rows(Workspace *workspace) {
	int c;

	workspace->rows++;

	for (c = 0; c < workspace->cols; c++) {
		workspace->table[c] = realloc(workspace->table[c], sizeof(Container*) * workspace->rows);
		new_container(&(workspace->table[c][workspace->rows-1]));
	}
}

/*
 * Add one column to the table
 *
 */
void expand_table_cols(Workspace *workspace) {
	int c;

	workspace->cols++;

	workspace->table = realloc(workspace->table, sizeof(Container**) * workspace->cols);
	workspace->table[workspace->cols-1] = calloc(sizeof(Container*) * workspace->rows, 1);
	for (c = 0; c < workspace->rows; c++)
		new_container(&(workspace->table[workspace->cols-1][c]));
}

/*
 * Performs simple bounds checking for the given column/row
 *
 */
bool cell_exists(int col, int row) {
	return (col >= 0 && col < c_ws->rows) &&
		(row >= 0 && row < c_ws->cols);
}
