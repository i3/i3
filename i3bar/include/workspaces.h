#ifndef WORKSPACES_H_
#define WORKSPACES_H_

#include "common.h"
#include "outputs.h"

typedef struct i3_ws i3_ws;

TAILQ_HEAD(ws_head, i3_ws);

void parse_workspaces_json();
void free_workspaces();

struct i3_ws {
	int			num;
	char			*name;
	int			name_width;
	bool			visible;
	bool			focused;
	bool			urgent;
	rect			rect;
	struct i3_output	*output;

	TAILQ_ENTRY(i3_ws)	tailq;
};

#endif
