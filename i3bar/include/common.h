/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 */
#pragma once

#include <config.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include "libi3.h"
#include "queue.h"

typedef struct rect_t rect;

extern struct ev_loop *main_loop;

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

/* This data structure describes the way a status block should be rendered. These
 * variables are updated each time the statusline is re-rendered. */
struct status_block_render_desc {
    uint32_t width;
    uint32_t x_offset;
    uint32_t x_append;
};

/* This data structure represents one JSON dictionary, multiple of these make
 * up one status line. */
struct status_block {
    i3String *full_text;
    i3String *short_text;

    bool use_short;
    uint32_t render_length;

    char *color;
    char *background;
    char *border;

    /* min_width can be specified either as a numeric value (in pixels) or as a
     * string. For strings, we set min_width to the measured text width of
     * min_width_str. */
    uint32_t min_width;
    char *min_width_str;

    blockalign_t align;

    bool urgent;
    bool no_separator;
    uint32_t border_top;
    uint32_t border_right;
    uint32_t border_bottom;
    uint32_t border_left;
    bool pango_markup;

    /* The amount of pixels necessary to render a separator after the block. */
    uint32_t sep_block_width;

    /* Continuously-updated information on how to render this status block. */
    struct status_block_render_desc full_render;
    struct status_block_render_desc short_render;

    /* Optional */
    char *name;
    char *instance;

    TAILQ_ENTRY(status_block) blocks;
};

extern TAILQ_HEAD(statusline_head, status_block) statusline_head;

#include "child.h"
#include "ipc.h"
#include "outputs.h"
#include "util.h"
#include "workspaces.h"
#include "mode.h"
#include "trayclients.h"
#include "xcb.h"
#include "configuration.h"
#include "parse_json_header.h"
