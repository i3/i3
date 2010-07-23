#ifndef WORKSPACES_H_
#define WORKSPACES_H_

#include "common.h"
#include "outputs.h"

typedef struct i3_ws_t i3_ws;

i3_ws* workspaces;

void parse_workspaces_json();
void free_workspaces();

struct i3_ws_t {
	int		num;
	char*		name;
	bool		visible;
	bool		focused;
	bool		urgent;
	rect		rect;
	i3_output*	output;

	i3_ws*		next;
};

#endif
