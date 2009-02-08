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

void init_table() {
	expand_table_cols();
	expand_table_rows();
}

void expand_table_rows() {
	int c;
	Container *new;

	table_dims.y++;

	for (c = 0; c < table_dims.x; c++) {
		table[c] = realloc(table[c], sizeof(Container*) * table_dims.y);
		new = table[c][table_dims.y-1] = calloc(sizeof(Container), 1);
		CIRCLEQ_INIT(&(new->clients));
	}
}

void expand_table_cols() {
	int c;
	Container *new;

	table_dims.x++;
	table = realloc(table, sizeof(Container**) * table_dims.x);
	table[table_dims.x-1] = calloc(sizeof(Container*) * table_dims.y, 1);
	for (c = 0; c < table_dims.y; c++) {
		new = table[table_dims.x-1][c] = calloc(sizeof(Container), 1);
		CIRCLEQ_INIT(&(new->clients));
	}
}

/*
 * Performs simple bounds checking for the given column/row
 *
 */
bool cell_exists(int col, int row) {
	return (col >= 0 && col < table_dims.x) &&
		(row >= 0 && row < table_dims.y);
}

