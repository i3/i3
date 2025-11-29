/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * config.c: Parses the configuration (received from i3).
 *
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

config_t config = {0};

/*
 * Parse the bindings array
 */
static void parse_bindings(yyjson_val *bindings_arr) {
    if (!yyjson_is_arr(bindings_arr)) {
        return;
    }

    size_t idx, max;
    yyjson_val *binding_obj;
    yyjson_arr_foreach(bindings_arr, idx, max, binding_obj) {
        if (!yyjson_is_obj(binding_obj)) {
            continue;
        }

        binding_t *binding = scalloc(1, sizeof(binding_t));
        binding->input_code = yyjson_get_int(yyjson_obj_get(binding_obj, "input_code"));
        const char *cmd = yyjson_get_str(yyjson_obj_get(binding_obj, "command"));
        if (cmd) {
            binding->command = sstrdup(cmd);
        }
        binding->release = yyjson_get_bool(yyjson_obj_get(binding_obj, "release"));
        TAILQ_INSERT_TAIL(&(config.bindings), binding, bindings);
    }
}

/*
 * Parse the tray_outputs array
 */
static void parse_tray_outputs(yyjson_val *tray_arr) {
    if (!yyjson_is_arr(tray_arr)) {
        return;
    }

    size_t idx, max;
    yyjson_val *output_val;
    yyjson_arr_foreach(tray_arr, idx, max, output_val) {
        if (!yyjson_is_str(output_val)) {
            continue;
        }
        const char *output = unsafe_yyjson_get_str(output_val);
        size_t len = unsafe_yyjson_get_len(output_val);
        DLOG("Adding tray_output = %.*s to the list.\n", (int)len, output);
        tray_output_t *tray_output = scalloc(1, sizeof(tray_output_t));
        tray_output->output = sstrdup(output);
        TAILQ_INSERT_TAIL(&(config.tray_outputs), tray_output, tray_outputs);
    }
}

/*
 * Parse the outputs array
 */
static void parse_outputs(yyjson_val *outputs_arr) {
    if (!yyjson_is_arr(outputs_arr)) {
        return;
    }

    size_t idx, max;
    yyjson_val *output_val;
    yyjson_arr_foreach(outputs_arr, idx, max, output_val) {
        if (!yyjson_is_str(output_val)) {
            continue;
        }
        const char *output = unsafe_yyjson_get_str(output_val);
        size_t len = unsafe_yyjson_get_len(output_val);
        DLOG("+output %.*s\n", (int)len, output);
        int new_num_outputs = config.num_outputs + 1;
        config.outputs = srealloc(config.outputs, sizeof(char *) * new_num_outputs);
        config.outputs[config.num_outputs] = sstrdup(output);
        config.num_outputs = new_num_outputs;
    }
}

/*
 * Parse the padding rect
 */
static void parse_padding(yyjson_val *padding_obj) {
    if (!yyjson_is_obj(padding_obj)) {
        return;
    }
    config.padding.x = yyjson_get_int(yyjson_obj_get(padding_obj, "x"));
    config.padding.y = yyjson_get_int(yyjson_obj_get(padding_obj, "y"));
    config.padding.width = yyjson_get_int(yyjson_obj_get(padding_obj, "width"));
    config.padding.height = yyjson_get_int(yyjson_obj_get(padding_obj, "height"));
    DLOG("padding = {x=%d, y=%d, width=%d, height=%d}\n",
         config.padding.x, config.padding.y, config.padding.width, config.padding.height);
}

/*
 * Parse the colors object
 */
static void parse_colors(yyjson_val *colors_obj) {
    if (!yyjson_is_obj(colors_obj)) {
        return;
    }

#define PARSE_COLOR(json_name, struct_name)                                            \
    do {                                                                               \
        yyjson_val *val = yyjson_obj_get(colors_obj, #json_name);                      \
        if (val && yyjson_is_str(val)) {                                               \
            DLOG(#json_name " = " #struct_name " = %s\n", unsafe_yyjson_get_str(val)); \
            config.colors.struct_name = sstrdup(unsafe_yyjson_get_str(val));           \
        }                                                                              \
    } while (0)

    PARSE_COLOR(statusline, bar_fg);
    PARSE_COLOR(background, bar_bg);
    PARSE_COLOR(separator, sep_fg);
    PARSE_COLOR(focused_statusline, focus_bar_fg);
    PARSE_COLOR(focused_background, focus_bar_bg);
    PARSE_COLOR(focused_separator, focus_sep_fg);
    PARSE_COLOR(focused_workspace_border, focus_ws_border);
    PARSE_COLOR(focused_workspace_bg, focus_ws_bg);
    PARSE_COLOR(focused_workspace_text, focus_ws_fg);
    PARSE_COLOR(active_workspace_border, active_ws_border);
    PARSE_COLOR(active_workspace_bg, active_ws_bg);
    PARSE_COLOR(active_workspace_text, active_ws_fg);
    PARSE_COLOR(inactive_workspace_border, inactive_ws_border);
    PARSE_COLOR(inactive_workspace_bg, inactive_ws_bg);
    PARSE_COLOR(inactive_workspace_text, inactive_ws_fg);
    PARSE_COLOR(urgent_workspace_border, urgent_ws_border);
    PARSE_COLOR(urgent_workspace_bg, urgent_ws_bg);
    PARSE_COLOR(urgent_workspace_text, urgent_ws_fg);
    PARSE_COLOR(binding_mode_border, binding_mode_border);
    PARSE_COLOR(binding_mode_bg, binding_mode_bg);
    PARSE_COLOR(binding_mode_text, binding_mode_fg);

#undef PARSE_COLOR
}

/*
 * Parse the received bar configuration JSON string
 *
 */
void parse_config_json(const unsigned char *json, size_t size) {
    TAILQ_INIT(&(config.bindings));
    TAILQ_INIT(&(config.tray_outputs));

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, size, 0, NULL, &err);
    if (!doc) {
        ELOG("JSON parse error for config: %s (at position %zu)\n", err.msg, err.pos);
        exit(EXIT_FAILURE);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        ELOG("Could not parse config reply: not an object\n");
        yyjson_doc_free(doc);
        exit(EXIT_FAILURE);
    }

    /* Check if id is null (bar config not found) */
    yyjson_val *id_val = yyjson_obj_get(root, "id");
    if (yyjson_is_null(id_val)) {
        ELOG("No such bar config. Use 'i3-msg -t get_bar_config' to get the available configs.\n");
        ELOG("Are you starting i3bar by hand? You should not:\n");
        ELOG("Configure a 'bar' block in your i3 config and i3 will launch i3bar automatically.\n");
        yyjson_doc_free(doc);
        exit(EXIT_FAILURE);
    }

    /* Parse mode */
    const char *mode = yyjson_get_str(yyjson_obj_get(root, "mode"));
    if (mode) {
        DLOG("mode = %s\n", mode);
        if (strcmp(mode, "dock") == 0) {
            config.hide_on_modifier = M_DOCK;
        } else if (strcmp(mode, "hide") == 0) {
            config.hide_on_modifier = M_HIDE;
        } else {
            config.hide_on_modifier = M_INVISIBLE;
        }
    }

    /* Parse hidden_state */
    const char *hidden = yyjson_get_str(yyjson_obj_get(root, "hidden_state"));
    if (hidden) {
        DLOG("hidden_state = %s\n", hidden);
        config.hidden_state = (strcmp(hidden, "hide") == 0) ? S_HIDE : S_SHOW;
    }

    /* Parse modifier - can be int or string for backwards compatibility */
    yyjson_val *modifier_val = yyjson_obj_get(root, "modifier");
    if (yyjson_is_int(modifier_val)) {
        config.modifier = unsafe_yyjson_get_int(modifier_val);
        DLOG("modifier = %d\n", config.modifier);
    } else {
        const char *mod = yyjson_get_str(modifier_val);
        if (mod) {
            DLOG("modifier = %s\n", mod);
            if (strcmp(mod, "none") == 0) {
                config.modifier = XCB_NONE;
            } else if (strcmp(mod, "shift") == 0) {
                config.modifier = XCB_MOD_MASK_SHIFT;
            } else if (strcmp(mod, "ctrl") == 0) {
                config.modifier = XCB_MOD_MASK_CONTROL;
            } else if (strcmp(mod, "Mod1") == 0) {
                config.modifier = XCB_MOD_MASK_1;
            } else if (strcmp(mod, "Mod2") == 0) {
                config.modifier = XCB_MOD_MASK_2;
            } else if (strcmp(mod, "Mod3") == 0) {
                config.modifier = XCB_MOD_MASK_3;
            } else if (strcmp(mod, "Mod5") == 0) {
                config.modifier = XCB_MOD_MASK_5;
            } else {
                config.modifier = XCB_MOD_MASK_4;
            }
        }
    }

    /* Parse position */
    const char *pos = yyjson_get_str(yyjson_obj_get(root, "position"));
    if (pos) {
        DLOG("position = %s\n", pos);
        config.position = (strcmp(pos, "top") == 0) ? POS_TOP : POS_BOT;
    }

    /* Parse status_command */
    const char *status_cmd = yyjson_get_str(yyjson_obj_get(root, "status_command"));
    if (status_cmd) {
        DLOG("status_command = %s\n", status_cmd);
        config.command = sstrdup(status_cmd);
    }

    /* Parse workspace_command */
    const char *ws_cmd = yyjson_get_str(yyjson_obj_get(root, "workspace_command"));
    if (ws_cmd) {
        DLOG("workspace_command = %s\n", ws_cmd);
        config.workspace_command = sstrdup(ws_cmd);
    }

    /* Parse font */
    const char *font = yyjson_get_str(yyjson_obj_get(root, "font"));
    if (font) {
        DLOG("font = %s\n", font);
        FREE(config.fontname);
        config.fontname = sstrdup(font);
    }

    /* Parse separator_symbol */
    const char *sep = yyjson_get_str(yyjson_obj_get(root, "separator_symbol"));
    if (sep) {
        DLOG("separator = %s\n", sep);
        I3STRING_FREE(config.separator_symbol);
        config.separator_symbol = i3string_from_utf8(sep);
    }

    /* Parse integer options */
    config.bar_height = yyjson_get_int(yyjson_obj_get(root, "bar_height"));
    config.tray_padding = yyjson_get_int(yyjson_obj_get(root, "tray_padding"));
    config.ws_min_width = yyjson_get_int(yyjson_obj_get(root, "workspace_min_width"));
    DLOG("bar_height=%d, tray_padding=%d, workspace_min_width=%d\n",
         config.bar_height, config.tray_padding, config.ws_min_width);

    /* Parse boolean options */
    config.disable_binding_mode_indicator = !yyjson_get_bool(yyjson_obj_get(root, "binding_mode_indicator"));
    config.disable_ws = !yyjson_get_bool(yyjson_obj_get(root, "workspace_buttons"));
    config.strip_ws_numbers = yyjson_get_bool(yyjson_obj_get(root, "strip_workspace_numbers"));
    config.strip_ws_name = yyjson_get_bool(yyjson_obj_get(root, "strip_workspace_name"));
    if (!config.verbose) {
        config.verbose = yyjson_get_bool(yyjson_obj_get(root, "verbose"));
    }
    DLOG("binding_mode_indicator=%d, workspace_buttons=%d, strip_ws_numbers=%d, strip_ws_name=%d, verbose=%d\n",
         !config.disable_binding_mode_indicator, !config.disable_ws, config.strip_ws_numbers, config.strip_ws_name, config.verbose);

    /* Parse backwards-compat wheel commands */
    const char *wheel_up = yyjson_get_str(yyjson_obj_get(root, "wheel_up_cmd"));
    if (wheel_up) {
        DLOG("wheel_up_cmd = %s\n", wheel_up);
        binding_t *binding = scalloc(1, sizeof(binding_t));
        binding->input_code = 4;
        binding->command = sstrdup(wheel_up);
        TAILQ_INSERT_TAIL(&(config.bindings), binding, bindings);
    }

    const char *wheel_down = yyjson_get_str(yyjson_obj_get(root, "wheel_down_cmd"));
    if (wheel_down) {
        DLOG("wheel_down_cmd = %s\n", wheel_down);
        binding_t *binding = scalloc(1, sizeof(binding_t));
        binding->input_code = 5;
        binding->command = sstrdup(wheel_down);
        TAILQ_INSERT_TAIL(&(config.bindings), binding, bindings);
    }

    /* Parse deprecated single tray_output */
    const char *tray_output = yyjson_get_str(yyjson_obj_get(root, "tray_output"));
    if (tray_output) {
        DLOG("Found deprecated key tray_output %s.\n", tray_output);
        tray_output_t *to = scalloc(1, sizeof(tray_output_t));
        to->output = sstrdup(tray_output);
        TAILQ_INSERT_TAIL(&(config.tray_outputs), to, tray_outputs);
    }

    /* Parse arrays */
    parse_bindings(yyjson_obj_get(root, "bindings"));
    parse_tray_outputs(yyjson_obj_get(root, "tray_outputs"));
    parse_outputs(yyjson_obj_get(root, "outputs"));
    parse_padding(yyjson_obj_get(root, "padding"));
    parse_colors(yyjson_obj_get(root, "colors"));

    yyjson_doc_free(doc);

    if (config.disable_ws && config.workspace_command) {
        ELOG("You have specified 'workspace_buttons no'. Your 'workspace_command %s' will be ignored.\n", config.workspace_command);
        FREE(config.workspace_command);
    }
}

/*
 * Parse the received bar configuration list. The only usecase right now is to
 * automatically get the first bar id.
 *
 */
void parse_get_first_i3bar_config(const unsigned char *json, size_t size) {
    yyjson_doc *doc = yyjson_read_opts((char *)json, size, 0, NULL, NULL);
    if (!doc) {
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (yyjson_is_arr(root)) {
        yyjson_val *first = yyjson_arr_get_first(root);
        if (first && yyjson_is_str(first)) {
            config.bar_id = sstrdup(unsafe_yyjson_get_str(first));
        }
    }

    yyjson_doc_free(doc);
}

/*
 * free()s the color strings as soon as they are not needed anymore.
 *
 */
void free_colors(struct xcb_color_strings_t *colors) {
#define FREE_COLOR(x)    \
    do {                 \
        FREE(colors->x); \
    } while (0)
    FREE_COLOR(bar_fg);
    FREE_COLOR(bar_bg);
    FREE_COLOR(sep_fg);
    FREE_COLOR(focus_bar_fg);
    FREE_COLOR(focus_bar_bg);
    FREE_COLOR(focus_sep_fg);
    FREE_COLOR(active_ws_fg);
    FREE_COLOR(active_ws_bg);
    FREE_COLOR(active_ws_border);
    FREE_COLOR(inactive_ws_fg);
    FREE_COLOR(inactive_ws_bg);
    FREE_COLOR(inactive_ws_border);
    FREE_COLOR(urgent_ws_fg);
    FREE_COLOR(urgent_ws_bg);
    FREE_COLOR(urgent_ws_border);
    FREE_COLOR(focus_ws_fg);
    FREE_COLOR(focus_ws_bg);
    FREE_COLOR(focus_ws_border);
    FREE_COLOR(binding_mode_fg);
    FREE_COLOR(binding_mode_bg);
    FREE_COLOR(binding_mode_border);
#undef FREE_COLOR
}
