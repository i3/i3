#ifndef COMMON_H_
#define COMMON_H_

#include "util.h"

typedef int bool;

typedef struct rect_t rect;

struct rect_t {
	int	x;
	int	y;
	int	w;
	int	h;
};

struct ev_loop* main_loop;

#endif
