/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * outputs.c: Maintaining the output-list
 *
 */
#pragma once

#include <xcb/xcb.h>

#include "common.h"

typedef struct i3_output i3_output;

SLIST_HEAD(outputs_head, i3_output);
struct outputs_head* outputs;

/*
 * Start parsing the received json-string
 *
 */
void parse_outputs_json(char* json);

/*
 * Initiate the output-list
 *
 */
void init_outputs(void);

/*
 * Returns the output with the given name
 *
 */
i3_output* get_output_by_name(char* name);

struct i3_output {
    char* name;   /* Name of the output */
    bool active;  /* If the output is active */
    bool primary; /* If it is the primary output */
    bool visible; /* If the bar is visible on this output */
    int ws;       /* The number of the currently visible ws */
    rect rect;    /* The rect (relative to the root-win) */

    xcb_window_t bar;     /* The id of the bar of the output */
    xcb_pixmap_t buffer;  /* An extra pixmap for double-buffering */
    xcb_gcontext_t bargc; /* The graphical context of the bar */

    struct ws_head* workspaces;  /* The workspaces on this output */
    struct tc_head* trayclients; /* The tray clients on this output */

    SLIST_ENTRY(i3_output) slist; /* Pointer for the SLIST-Macro */
};
