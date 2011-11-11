/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2011 Axel Wagner and contributors (see also: LICENSE)
 *
 */
#ifndef COMMON_H_
#define COMMON_H_

#include <stdbool.h>

typedef struct rect_t rect;

struct ev_loop* main_loop;
char            *statusline;
char            *statusline_buffer;

struct rect_t {
    int x;
    int y;
    int w;
    int h;
};

#include "queue.h"
#include "child.h"
#include "ipc.h"
#include "outputs.h"
#include "util.h"
#include "workspaces.h"
#include "trayclients.h"
#include "xcb.h"
#include "config.h"
#include "libi3.h"

#endif
