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
    new_output->name = NULL;
    new_output->active = false;
    new_output->primary = false;
    new_output->visible = false;
    new_output->ws = 0;
    new_output->statusline_width = 0;
    memset(&new_output->rect, 0, sizeof(rect));
    memset(&new_output->bar, 0, sizeof(surface_t));
    memset(&new_output->buffer, 0, sizeof(surface_t));
    memset(&new_output->statusline_buffer, 0, sizeof(surface_t));

    new_output->workspaces = smalloc(sizeof(struct ws_head));
    TAILQ_INIT(new_output->workspaces);

    new_output->trayclients = smalloc(sizeof(struct tc_head));
    TAILQ_INIT(new_output->trayclients);

    /* Parse name */
    yyjson_val *name_val = yyjson_obj_get(output_obj, "name");
    if (name_val && yyjson_is_str(name_val)) {
        new_output->name = sstrdup(unsafe_yyjson_get_str(name_val));
    }

    /* Parse active */
    yyjson_val *active_val = yyjson_obj_get(output_obj, "active");
    if (active_val && yyjson_is_bool(active_val)) {
        new_output->active = unsafe_yyjson_get_bool(active_val);
    }

    /* Parse primary */
    yyjson_val *primary_val = yyjson_obj_get(output_obj, "primary");
    if (primary_val && yyjson_is_bool(primary_val)) {
        new_output->primary = unsafe_yyjson_get_bool(primary_val);
    }

    /* Parse current_workspace */
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
    yyjson_val *rect_val = yyjson_obj_get(output_obj, "rect");
    if (rect_val && yyjson_is_obj(rect_val)) {
        yyjson_val *x_val = yyjson_obj_get(rect_val, "x");
        if (x_val && yyjson_is_int(x_val)) {
            new_output->rect.x = unsafe_yyjson_get_int(x_val);
        }
        yyjson_val *y_val = yyjson_obj_get(rect_val, "y");
        if (y_val && yyjson_is_int(y_val)) {
            new_output->rect.y = unsafe_yyjson_get_int(y_val);
        }
        yyjson_val *w_val = yyjson_obj_get(rect_val, "width");
        if (w_val && yyjson_is_int(w_val)) {
            new_output->rect.w = unsafe_yyjson_get_int(w_val);
        }
        yyjson_val *h_val = yyjson_obj_get(rect_val, "height");
        if (h_val && yyjson_is_int(h_val)) {
            new_output->rect.h = unsafe_yyjson_get_int(h_val);
        }
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
