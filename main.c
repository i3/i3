#include <termios.h>
#include <unistd.h>
#include <stdio.h>

enum { move, snap, select };

struct foo {
	int width;
	int height;
	int x;
	int y;
};


int main() {
	int c;
	int mode;
	struct termios tbuf, obuf;
	if (tcgetattr(STDIN_FILENO, &obuf) < 0 ||
		tcgetattr(STDIN_FILENO, &tbuf) < 0)
		perror("tcgetattr()");
	tbuf.c_lflag &= ~ICANON;
	tbuf.c_cc[VMIN] = 1;
	tbuf.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &tbuf);

	int table[10][10];
	int colspan[10];
	int rowspan[10];
	int current_row = 0;
	int current_col = 0;
	int current = -1;
	int created_terms = 0;

	for (c = 0; c < 10; c++) {
		int i;
		for (i = 0; i < 10; i++)
			table[c][i] = -1;
		colspan[c] = 1;
		rowspan[c] = 1;
	}

	mode = select;
	while (printf("%s> ", (mode == move ? "move" : (mode == select ? "select" : "snap"))), c = getchar()) {

		printf("char %c, %d\n", c, c);
		if (c == 'm')
			mode = move;
		else if (c == 's')
			mode = snap;
		else if (c == 'u') {
			/* insert new 'terminal' below current one */
			int i;
			printf("current row = %d\n", current_row);
			printf("current col = %d\n", current_col);
			for (i = current_row; i < 10; i++) {
				if (table[current_col][i] == -1) {
					printf("found empty entry at %d\n", i);
					created_terms++;
					table[current_col][i] = created_terms;
					current_row = i;
					printf("created terminal %d\n", created_terms);
					printf("current_row = %d\n", current_row);
					break;
				}
			}
		}
		else if (c == 'n') {
			if (mode == move) {
				printf("move window left\n");
				table[current_col-1][current_row] = table[current_col][current_row];
				table[current_col][current_row] = -1;

			} else if (mode == snap) {
				printf("snap window left\n");
			} else if (mode == select) {
				printf("go to left window\n");
				if (current_col > 0)
					current_col--;
				printf("col now: %d\n", current_col);
			}
			mode = select;

		}
		else if (c == 'r') {
			if (mode == move) {
				printf("move window down\n");
				table[current_col][current_row+1] = table[current_col][current_row];
				table[current_col][current_row] = -1;

			} else if (mode == snap) {
				printf("snap window down\n");
			} else if (mode == select) {
				printf("go to window below\n");
				if (current_row < 9)
					current_row++;
				printf("row now: %d\n", current_row);
			}
			mode = select;

		}
		else if (c == 't') {
			if (mode == move) {
				printf("move window up\n");
				table[current_col][current_row-1] = table[current_col][current_row];
				table[current_col][current_row] = -1;
			} else if (mode == snap) {
				printf("snap window up\n");
			} else if (mode == select) {
				printf("go to upper window\n");
				if (current_row > 0)
					current_row--;
				printf("row now: %d\n", current_row);
			}
			mode = select;

		}
		else if (c == 'd') {
			if (mode == move) {
				printf("move window right\n");
				table[current_col+1][current_row] = table[current_col][current_row];
				table[current_col][current_row] = -1;
				current_col++;
			} else if (mode == snap) {
				printf("snap window right\n");
				colspan[table[current_col][current_row]]++;
				printf("colspan now is: %d\n", colspan[table[current_col][current_row]]++);

			} else if (mode == select) {
				printf("go to right window\n");
				if (current_col < 9)
					current_col++;
				printf("col now: %d\n", current_col);
			}
			mode = select;

		}

		int  rows, cols;
		printf("your windows are as following:\n");
		system("/tmp/killgeom.sh");
		for (rows = 0; rows < 10; rows++)
			for (cols = 0; cols < 10; cols++) {
				if (table[cols][rows] != -1) {
				printf("client %d, x = %d, y = %d, width = %d, height = %d",
						table[cols][rows], cols * 60, rows * 60, 15 * 1, 15 * 1);
				if (cols == current_col && rows == current_row)
					printf("   < ===== YOU ARE HERE\n");
				else printf("\n");
				char *buffer;
				asprintf(&buffer, "/bin/sh -c \"urxvt -geometry %dx%d+%d+%d %s&\"",
				15 * colspan[table[cols][rows]], 15, cols * 200, rows * 200, (cols == current_col && rows == current_row ? "-bg white" : "-bg gray"));
				printf("executing %s\n", buffer);

				system(buffer);
				free(buffer);
				}
			}
		printf("that's all\n");
	}
}
