/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * workspaces.c: Maintaining the workspace lists
 *
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

/*
 * Parse a single workspace object
 */
static void parse_workspace_object(yyjson_val *ws_obj) {
    if (!yyjson_is_obj(ws_obj)) {
        return;
    }

    i3_ws *new_workspace = scalloc(1, sizeof(i3_ws));
    new_workspace->num = -1;

    /* Parse id */
    yyjson_val *id_val = yyjson_obj_get(ws_obj, "id");
    if (id_val && yyjson_is_int(id_val)) {
        new_workspace->id = yyjson_get_int(id_val);
    }

    /* Parse num */
    yyjson_val *num_val = yyjson_obj_get(ws_obj, "num");
    if (num_val && yyjson_is_int(num_val)) {
        new_workspace->num = yyjson_get_int(num_val);
    }

    /* Parse visible */
    yyjson_val *visible_val = yyjson_obj_get(ws_obj, "visible");
    if (visible_val && yyjson_is_bool(visible_val)) {
        new_workspace->visible = yyjson_get_bool(visible_val);
    }

    /* Parse focused */
    yyjson_val *focused_val = yyjson_obj_get(ws_obj, "focused");
    if (focused_val && yyjson_is_bool(focused_val)) {
        new_workspace->focused = yyjson_get_bool(focused_val);
    }

    /* Parse urgent */
    yyjson_val *urgent_val = yyjson_obj_get(ws_obj, "urgent");
    if (urgent_val && yyjson_is_bool(urgent_val)) {
        new_workspace->urgent = yyjson_get_bool(urgent_val);
    }

    /* Parse name */
    yyjson_val *name_val = yyjson_obj_get(ws_obj, "name");
    if (name_val && yyjson_is_str(name_val)) {
        const char *ws_name = yyjson_get_str(name_val);
        size_t len = yyjson_get_len(name_val);
        new_workspace->canonical_name = sstrndup(ws_name, len);

        if ((config.strip_ws_numbers || config.strip_ws_name) && new_workspace->num >= 0) {
            /* Special case: strip off the workspace number/name */
            static char ws_num[32];

            snprintf(ws_num, sizeof(ws_num), "%d", new_workspace->num);

            /* Calculate the length of the number str in the name */
            size_t offset = strspn(ws_name, ws_num);

            /* Also strip off the conventional ws name delimiter */
            if (offset && ws_name[offset] == ':') {
                offset += 1;
            }

            if (config.strip_ws_numbers) {
                /* Offset may be equal to length, in which case display the number */
                new_workspace->name = offset < len
                                          ? i3string_from_markup_with_length(ws_name + offset, len - offset)
                                          : i3string_from_markup(ws_num);
            } else {
                new_workspace->name = i3string_from_markup(ws_num);
            }
        } else {
            /* Default case: just save the name */
            new_workspace->name = i3string_from_markup_with_length(ws_name, len);
        }

        /* Save its rendered width */
        new_workspace->name_width = predict_text_width(new_workspace->name);

        DLOG("Got workspace canonical: %s, name: '%s', name_width: %d, glyphs: %zu\n",
             new_workspace->canonical_name,
             i3string_as_utf8(new_workspace->name),
             new_workspace->name_width,
             i3string_get_num_glyphs(new_workspace->name));
    }

    /* Parse output and add to output's workspace list */
    yyjson_val *output_val = yyjson_obj_get(ws_obj, "output");
    if (output_val && yyjson_is_str(output_val)) {
        const char *output_name = yyjson_get_str(output_val);
        i3_output *target = get_output_by_name((char *)output_name);
        if (target != NULL) {
            new_workspace->output = target;
            TAILQ_INSERT_TAIL(new_workspace->output->workspaces, new_workspace, tailq);
            return;
        }
    }

    /* If no output was assigned, check for valid state */
    if (!new_workspace->name || SLIST_EMPTY(outputs)) {
        I3STRING_FREE(new_workspace->name);
        FREE(new_workspace->canonical_name);
        FREE(new_workspace);
        return;
    }

    /* Handle no output case - assign to primary or first output */
    new_workspace->output = get_output_by_name("primary");
    if (new_workspace->output == NULL) {
        new_workspace->output = SLIST_FIRST(outputs);
    }
    TAILQ_INSERT_TAIL(new_workspace->output->workspaces, new_workspace, tailq);
}

/*
 * Parse the received JSON string
 *
 */
void parse_workspaces_json(const unsigned char *json, const size_t size) {
    free_workspaces();

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, size, 0, NULL, &err);
    if (!doc) {
        ELOG("JSON parse error for workspaces: %s (at position %zu), json:---%.*s---\n", err.msg, err.pos, (int)size, (char *)json);
        if (config.workspace_command) {
            kill_ws_child();
            set_workspace_button_error("Could not parse workspace_command's JSON");
        } else {
            exit(EXIT_FAILURE);
        }
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        ELOG("Could not parse workspaces reply: not an array\n");
        yyjson_doc_free(doc);
        if (config.workspace_command) {
            kill_ws_child();
            set_workspace_button_error("Could not parse workspace_command's JSON");
        } else {
            exit(EXIT_FAILURE);
        }
        return;
    }

    size_t idx, max;
    yyjson_val *ws_obj;
    yyjson_arr_foreach(root, idx, max, ws_obj) {
        parse_workspace_object(ws_obj);
    }

    yyjson_doc_free(doc);
}

/*
 * free() all workspace data structures. Does not free() the heads of the tailqueues.
 *
 */
void free_workspaces(void) {
    if (outputs == NULL) {
        return;
    }

    i3_output *outputs_walk;
    SLIST_FOREACH (outputs_walk, outputs, slist) {
        if (outputs_walk->workspaces != NULL && !TAILQ_EMPTY(outputs_walk->workspaces)) {
            i3_ws *ws_walk;
            TAILQ_FOREACH (ws_walk, outputs_walk->workspaces, tailq) {
                I3STRING_FREE(ws_walk->name);
                FREE(ws_walk->canonical_name);
            }
            FREE_TAILQ(outputs_walk->workspaces, i3_ws);
        }
    }
}
