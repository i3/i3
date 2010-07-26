#ifndef OUTPUTS_H_
#define OUTPUTS_H_

#include "common.h"
#include <xcb/xcb.h>

typedef struct i3_output_t i3_output;

i3_output* outputs;

void		parse_outputs_json(char* json);
void		free_outputs();
i3_output*	get_output_by_name(char* name);

struct i3_output_t {
	char*		name;
	bool		active;
	int		ws;
	rect		rect;

	xcb_window_t	bar;
	xcb_gcontext_t	bargc;

	i3_output*	next;
};

#endif
