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

/* This is a two-dimensional dynamic array of Container-pointers. I’ve always wanted
 * to be a three-star programmer :) */
Container ***table = NULL;

struct table_dimensions_t table_dims = {0, 0};

/*
 * Initialize table
 *
 */
void init_table() {
	expand_table_cols();
	expand_table_rows();
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
void expand_table_rows() {
	int c;

	table_dims.y++;

	for (c = 0; c < table_dims.x; c++) {
		table[c] = realloc(table[c], sizeof(Container*) * table_dims.y);
		new_container(&(table[c][table_dims.y-1]));
	}
}

/*
 * Add one column to the table
 *
 */
void expand_table_cols() {
	int c;

	table_dims.x++;
	table = realloc(table, sizeof(Container**) * table_dims.x);
	table[table_dims.x-1] = calloc(sizeof(Container*) * table_dims.y, 1);
	for (c = 0; c < table_dims.y; c++)
		new_container(&(table[table_dims.x-1][c]));
}

/*
 * Performs simple bounds checking for the given column/row
 *
 */
bool cell_exists(int col, int row) {
	return (col >= 0 && col < table_dims.x) &&
		(row >= 0 && row < table_dims.y);
}
