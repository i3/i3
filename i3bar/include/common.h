#ifndef COMMON_H_
#define COMMON_H_

#include "util.h"

typedef struct rect_t rect;
typedef int bool;

struct ev_loop* main_loop;
pid_t           child_pid;
char            *statusline;

struct rect_t {
	int	x;
	int	y;
	int	w;
	int	h;
};

#endif
