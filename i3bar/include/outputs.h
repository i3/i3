#ifndef OUTPUTS_H_
#define OUTPUTS_H_

#include <xcb/xcb.h>

#include "common.h"

typedef struct i3_output i3_output;

SLIST_HEAD(outputs_head, i3_output);
struct outputs_head *outputs;

void        parse_outputs_json(char* json);
void        free_outputs();
i3_output*  get_output_by_name(char* name);

struct i3_output {
	char*           name;
	bool            active;
	int             ws;
	rect            rect;

	xcb_window_t    bar;
	xcb_gcontext_t  bargc;

	struct ws_head  *workspaces;

	SLIST_ENTRY(i3_output) slist;
};

#endif
