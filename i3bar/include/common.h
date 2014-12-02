/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2011 Axel Wagner and contributors (see also: LICENSE)
 *
 */
#pragma once

#include <stdbool.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include "libi3.h"
#include "queue.h"

typedef struct rect_t rect;

struct ev_loop *main_loop;
char *statusline;
char *statusline_buffer;

struct rect_t {
    int x;
    int y;
    int w;
    int h;
};

typedef enum {
    /* First value to make it the default. */
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} blockalign_t;

/* This data structure represents one JSON dictionary, multiple of these make
 * up one status line. */
struct status_block {
    i3String *full_text;

    char *color;
    uint32_t min_width;
    blockalign_t align;

    bool urgent;
    bool no_separator;

    /* The amount of pixels necessary to render a separater after the block. */
    uint32_t sep_block_width;

    /* The amount of pixels necessary to render this block. These variables are
     * only temporarily used in refresh_statusline(). */
    uint32_t width;
    uint32_t x_offset;
    uint32_t x_append;

    /* Optional */
    char *name;
    char *instance;

    TAILQ_ENTRY(status_block) blocks;
};

TAILQ_HEAD(statusline_head, status_block) statusline_head;

#include "child.h"
#include "ipc.h"
#include "outputs.h"
#include "util.h"
#include "workspaces.h"
#include "mode.h"
#include "trayclients.h"
#include "xcb.h"
#include "config.h"
#include "libi3.h"
#include "parse_json_header.h"
