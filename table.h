#include <stdbool.h>
#include "data.h"

#ifndef _TABLE_H
#define _TABLE_H

#define CUR_TABLE (c_ws->table)
#define CUR_CELL (CUR_TABLE[current_col][current_row])

extern Workspace *c_ws;
extern Workspace workspaces[10];

void init_table();
void expand_table_rows(Workspace *workspace);
void expand_table_cols(Workspace *workspace);
bool cell_exists(int col, int row);

#endif
