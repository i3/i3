/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#ifndef WORKSPACES_H_
#define WORKSPACES_H_

#include <xcb/xproto.h>

#include "common.h"

typedef struct i3_ws i3_ws;

TAILQ_HEAD(ws_head, i3_ws);

/*
 * Start parsing the received json-string
 *
 */
void parse_workspaces_json();

/*
 * free() all workspace data-structures
 *
 */
void free_workspaces();

struct i3_ws {
    int                num;         /* The internal number of the ws */
    char               *name;       /* The name (in utf8) of the ws */
    xcb_char2b_t       *ucs2_name;  /* The name (in ucs2) of the ws */
    int                name_glyphs; /* The length (in glyphs) of the name */
    int                name_width;  /* The rendered width of the name */
    bool               visible;     /* If the ws is currently visible on an output */
    bool               focused;     /* If the ws is currently focused */
    bool               urgent;      /* If the urgent-hint of the ws is set */
    rect               rect;        /* The rect of the ws (not used (yet)) */
    struct i3_output   *output;     /* The current output of the ws */

    TAILQ_ENTRY(i3_ws) tailq;       /* Pointer for the TAILQ-Macro */
};

#endif
