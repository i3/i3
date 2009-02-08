#include <stdbool.h>
#include "data.h"

#ifndef _TABLE_H
#define _TABLE_H

#define CUR_CELL (table[current_col][current_row])

extern Container ***table;
extern struct table_dimensions_t table_dims;

void init_table();
void expand_table_rows();
void expand_table_cols();
bool cell_exists(int col, int row);

#endif
