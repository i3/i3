#include <stdio.h>
#include "table.h"

void print_table() {
    int r, c;
    printf("printing table...\n");
    for (c = 0; c < table_dims.x; c++)
        for (r = 0; r < table_dims.y; r++)
            printf("table[%d][%d] = %p\n", c, r, table[c][r]);
    printf("done\n");
}

int main() {
    printf("table.c tests\n");
    printf("table_dimensions = %d, %d\n", table_dims.x, table_dims.y);
    init_table();
    printf("table_dimensions = %d, %d\n", table_dims.x, table_dims.y);
    print_table();

    printf("expand_table_cols()\n");
    expand_table_cols();
    printf("table_dimensions = %d, %d\n", table_dims.x, table_dims.y);
    print_table();

    printf("expand_table_rows()\n");
    expand_table_rows();
    printf("table_dimensions = %d, %d\n", table_dims.x, table_dims.y);
    print_table();
}
