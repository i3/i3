/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * outputs.c: Maintaining the outputs list
 *
 */
#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

/* Simple JSON access macros - return default if missing or wrong type */
#define json_opt(obj, key, type, def)                                        \
    ({                                                                       \
        yyjson_val *_v = yyjson_obj_get(obj, key);                           \
        (_v && yyjson_is_##type(_v)) ? unsafe_yyjson_get_##type(_v) : (def); \
    })

#define json_opt_val(obj, key, type)               \
    ({                                             \
        yyjson_val *_v = yyjson_obj_get(obj, key); \
        (_v && yyjson_is_##type(_v)) ? _v : NULL;  \
    })

struct outputs_head *outputs;

static void clear_output(i3_output *output) {
    FREE(output->name);
    FREE(output->workspaces);
    FREE(output->trayclients);
}

/*
 * Initiate the outputs list
 *
 */
void init_outputs(void) {
    outputs = smalloc(sizeof(struct outputs_head));
    SLIST_INIT(outputs);
}

/*
 * Parse a single output object
 */
static void parse_output_object(yyjson_val *output_obj) {
    if (!yyjson_is_obj(output_obj)) {
        return;
    }

    i3_output *new_output = smalloc(sizeof(i3_output));
    memset(&new_output->rect, 0, sizeof(rect));
    memset(&new_output->bar, 0, sizeof(surface_t));
    memset(&new_output->buffer, 0, sizeof(surface_t));
    memset(&new_output->statusline_buffer, 0, sizeof(surface_t));
    new_output->statusline_width = 0;
    new_output->visible = false;

    new_output->workspaces = smalloc(sizeof(struct ws_head));
    TAILQ_INIT(new_output->workspaces);

    new_output->trayclients = smalloc(sizeof(struct tc_head));
    TAILQ_INIT(new_output->trayclients);

    const char *name = json_opt(output_obj, "name", str, NULL);
    new_output->name = name ? sstrdup(name) : NULL;
    new_output->active = json_opt(output_obj, "active", bool, false);
    new_output->primary = json_opt(output_obj, "primary", bool, false);

    /* Parse current_workspace - can be int or string */
    new_output->ws = 0;
    yyjson_val *ws_val = yyjson_obj_get(output_obj, "current_workspace");
    if (ws_val) {
        if (yyjson_is_int(ws_val)) {
            new_output->ws = unsafe_yyjson_get_int(ws_val);
        } else if (yyjson_is_str(ws_val)) {
            const char *ws_str = unsafe_yyjson_get_str(ws_val);
            char *end;
            errno = 0;
            long parsed_num = strtol(ws_str, &end, 10);
            if (errno == 0 && (end && *end == '\0')) {
                new_output->ws = parsed_num;
            }
        }
    }

    /* Parse rect */
    yyjson_val *rect_val = json_opt_val(output_obj, "rect", obj);
    if (rect_val) {
        new_output->rect.x = json_opt(rect_val, "x", int, 0);
        new_output->rect.y = json_opt(rect_val, "y", int, 0);
        new_output->rect.w = json_opt(rect_val, "width", int, 0);
        new_output->rect.h = json_opt(rect_val, "height", int, 0);
    }

    /* See if we actually handle that output */
    if (config.num_outputs > 0) {
        const bool is_primary = new_output->primary;
        bool handle_output = false;
        for (int c = 0; c < config.num_outputs; c++) {
            if ((strcasecmp(new_output->name, config.outputs[c]) == 0) ||
                (strcasecmp(config.outputs[c], "primary") == 0 && is_primary) ||
                (strcasecmp(config.outputs[c], "nonprimary") == 0 && !is_primary)) {
                handle_output = true;
                break;
            }
        }
        if (!handle_output) {
            DLOG("Ignoring output \"%s\", not configured to handle it.\n",
                 new_output->name);
            clear_output(new_output);
            FREE(new_output);
            return;
        }
    }

    i3_output *target = get_output_by_name(new_output->name);

    if (target == NULL) {
        SLIST_INSERT_HEAD(outputs, new_output, slist);
    } else {
        target->active = new_output->active;
        target->primary = new_output->primary;
        target->ws = new_output->ws;
        target->rect = new_output->rect;

        clear_output(new_output);
        FREE(new_output);
    }
}

/*
 * Parse the received JSON string
 *
 */
void parse_outputs_json(const unsigned char *json, size_t size) {
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, size, 0, NULL, &err);
    if (!doc) {
        ELOG("JSON parse error for outputs: %s (at position %zu)\n", err.msg, err.pos);
        exit(EXIT_FAILURE);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        ELOG("Could not parse outputs reply: not an array\n");
        yyjson_doc_free(doc);
        exit(EXIT_FAILURE);
    }

    size_t idx, max;
    yyjson_val *output_obj;
    yyjson_arr_foreach(root, idx, max, output_obj) {
        parse_output_object(output_obj);
    }

    yyjson_doc_free(doc);
}

/*
 * free() all outputs data structures.
 *
 */
void free_outputs(void) {
    free_workspaces();

    i3_output *outputs_walk;
    if (outputs == NULL) {
        return;
    }
    SLIST_FOREACH (outputs_walk, outputs, slist) {
        destroy_window(outputs_walk);
        if (outputs_walk->trayclients != NULL && !TAILQ_EMPTY(outputs_walk->trayclients)) {
            FREE_TAILQ(outputs_walk->trayclients, trayclient);
        }
        clear_output(outputs_walk);
    }
    FREE_SLIST(outputs, i3_output);
}

/*
 * Returns the output with the given name
 *
 */
i3_output *get_output_by_name(char *name) {
    if (name == NULL) {
        return NULL;
    }
    const bool is_primary = !strcasecmp(name, "primary");

    i3_output *walk;
    SLIST_FOREACH (walk, outputs, slist) {
        if ((is_primary && walk->primary) || !strcmp(walk->name, name)) {
            break;
        }
    }

    return walk;
}

/*
 * Returns true if the output has the currently focused workspace
 *
 */
bool output_has_focus(i3_output *output) {
    i3_ws *ws_walk;
    TAILQ_FOREACH (ws_walk, output->workspaces, tailq) {
        if (ws_walk->focused) {
            return true;
        }
    }
    return false;
}
