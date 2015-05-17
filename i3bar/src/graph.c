/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * config.c: Parses the configuration (received from i3).
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <i3/ipc.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

#include <X11/Xlib.h>

#include "graph.h"

TAILQ_HEAD(graphs_head, graph_t);

struct graphs_head graphs_ = TAILQ_HEAD_INITIALIZER(graphs_);
struct graphs_head* graphs = &graphs_;

static void free_graph(struct graph_t* graph) {
    if (graph->conn && graph->gradient) {
        xcb_free_pixmap(graph->conn, graph->gradient);
    }
    FREE(graph->values);
    FREE(graph->instance);
    FREE(graph);
}

struct graph_t* get_graph_and_mark(char* instance, char* gconfig) {
    struct graph_t* graph;
    graph_config_t* graph_config;
    uint32_t idx;
    TAILQ_FOREACH(graph_config, &config.graph_configs, configs) {
        if (!strcmp(gconfig, graph_config->graph_config)) {
            break;
        }
    }
    TAILQ_FOREACH(graph, graphs, entries) {
      if (0 == strcmp(graph->instance, instance)) {
        if (graph_config != graph->config) {
          free_graph(graph);
          break;
        }
        TAILQ_REMOVE(graphs, graph, entries);
        TAILQ_INSERT_TAIL(graphs, graph, entries);
        graph->marked = true;
        return graph;
      }
    }

    if (!graph_config)
      return NULL;

    graph = malloc(sizeof(struct graph_t));
    memset(graph, 0, sizeof(struct graph_t));
    graph->config = graph_config;
    graph->width = graph_config->width;
    graph->time_range = graph_config->time_range;
    graph->values = smalloc(sizeof(struct graph_data) * graph->width);
    for (idx = 0; idx < graph->width; ++idx) {
      graph->values[idx].value = graph->config->min;
      graph->values[idx].timestamp = 0;
    }

    graph->instance = strdup(instance);
    TAILQ_INSERT_TAIL(graphs, graph, entries);
    graph->marked = true;
    return graph;
}

void clean_up_marked_and_unmark() {
    while (!TAILQ_EMPTY(graphs) && !TAILQ_FIRST(graphs)->marked) {
        struct graph_t* graph = TAILQ_FIRST(graphs);
        TAILQ_REMOVE(graphs, graph, entries);
        free_graph(graph);
    }
    struct graph_t* graph;
    TAILQ_FOREACH(graph, graphs, entries) {
        graph->marked = false;
    }
}

void release_module() {
    while (!TAILQ_EMPTY(graphs)) {
        struct graph_t* graph = TAILQ_FIRST(graphs);
        TAILQ_REMOVE(graphs, graph, entries);
        free_graph(graph);
    }
}

void update_graph_with_value(struct graph_t* graph, uint32_t value, uint32_t timestamp) {
    int probes;
    uint32_t timediff;
    uint32_t last_value = graph->values[graph->width-1].value;
    uint32_t last_timestamp = graph->values[graph->width-1].timestamp;
    if (last_timestamp == 0) {
        graph->values[graph->width-1].timestamp = timestamp;
        graph->values[graph->width-1].value = value;
        return;
    }
    if (timestamp < last_timestamp)
        return;
    timediff = timestamp - last_timestamp;
    if (timediff < (graph->time_range/graph->width)) {
        graph->values[graph->width - 1].timestamp = timestamp;
        graph->values[graph->width - 1].value = value;
        return;
    }
    probes = (timediff*graph->width)/graph->config->time_range;
    for (uint32_t idx = 0; idx < graph->width - 1 - probes; ++idx) {
        graph->values[idx] = graph->values[idx + probes];
    }
    long long value_diff = (long long)value - (long long) last_value;
    int value_diff_step = value_diff / probes;
    uint32_t time_diff = timestamp - last_timestamp;
    uint32_t time_diff_step = time_diff/probes;
    for (int idx = 0; idx <= probes; ++idx) {
        int insert_idx = graph->width - idx - 1;
        uint32_t insert_value = value - value_diff_step;
        uint32_t insert_timestamp = timestamp - time_diff_step;
        graph->values[insert_idx].value = insert_value;
        graph->values[insert_idx].timestamp = insert_timestamp;
    }
}
