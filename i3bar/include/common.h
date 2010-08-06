#ifndef COMMON_H_
#define COMMON_H_

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

#include "queue.h"
#include "child.h"
#include "ipc.h"
#include "outputs.h"
#include "util.h"
#include "workspaces.h"
#include "xcb.h"
#include "ucs2_to_utf8.h"

#endif
