/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 */
#pragma once

#include <stdbool.h>
#include "libi3.h"
#include "queue.h"
#include "config.h"

struct graph_data {
    uint32_t value;
    uint32_t timestamp;
};

typedef uint32_t xcb_pixmap_t;
struct graph_t {
    char* instance;
    uint32_t width;
    uint32_t time_range;
    bool marked;
    graph_config_t* config;
    struct xcb_connection_t* conn;
    xcb_pixmap_t gradient;

    struct graph_data* values;
    TAILQ_ENTRY(graph_t) entries;
};

struct graph_t* get_graph_and_mark(char* instance, char* graph_config);
void clean_up_marked_and_unmark();
void release_module();
void update_graph_with_value(struct graph_t* graph, uint32_t value, uint32_t timestamp);
