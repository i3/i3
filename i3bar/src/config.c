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

config_t config;

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

        yyjson_val *input_code = yyjson_obj_get(binding_obj, "input_code");
        if (input_code && yyjson_is_int(input_code)) {
            binding->input_code = yyjson_get_int(input_code);
        }

        yyjson_val *command = yyjson_obj_get(binding_obj, "command");
        if (command && yyjson_is_str(command)) {
            binding->command = sstrdup(yyjson_get_str(command));
        }

        yyjson_val *release = yyjson_obj_get(binding_obj, "release");
        if (release && yyjson_is_bool(release)) {
            binding->release = yyjson_get_bool(release);
        }

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
        const char *output = yyjson_get_str(output_val);
        size_t len = yyjson_get_len(output_val);
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
        const char *output = yyjson_get_str(output_val);
        size_t len = yyjson_get_len(output_val);
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

    yyjson_val *x_val = yyjson_obj_get(padding_obj, "x");
    if (x_val && yyjson_is_int(x_val)) {
        config.padding.x = yyjson_get_int(x_val);
        DLOG("padding.x = %d\n", config.padding.x);
    }
    yyjson_val *y_val = yyjson_obj_get(padding_obj, "y");
    if (y_val && yyjson_is_int(y_val)) {
        config.padding.y = yyjson_get_int(y_val);
        DLOG("padding.y = %d\n", config.padding.y);
    }
    yyjson_val *width_val = yyjson_obj_get(padding_obj, "width");
    if (width_val && yyjson_is_int(width_val)) {
        config.padding.width = yyjson_get_int(width_val);
        DLOG("padding.width = %d\n", config.padding.width);
    }
    yyjson_val *height_val = yyjson_obj_get(padding_obj, "height");
    if (height_val && yyjson_is_int(height_val)) {
        config.padding.height = yyjson_get_int(height_val);
        DLOG("padding.height = %d\n", config.padding.height);
    }
}

/*
 * Parse the colors object
 */
static void parse_colors(yyjson_val *colors_obj) {
    if (!yyjson_is_obj(colors_obj)) {
        return;
    }

#define PARSE_COLOR(json_name, struct_name)                                     \
    do {                                                                        \
        yyjson_val *val = yyjson_obj_get(colors_obj, #json_name);               \
        if (val && yyjson_is_str(val)) {                                        \
            DLOG(#json_name " = " #struct_name " = %s\n", yyjson_get_str(val)); \
            config.colors.struct_name = sstrdup(yyjson_get_str(val));           \
        }                                                                       \
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
    if (id_val && yyjson_is_null(id_val)) {
        ELOG("No such bar config. Use 'i3-msg -t get_bar_config' to get the available configs.\n");
        ELOG("Are you starting i3bar by hand? You should not:\n");
        ELOG("Configure a 'bar' block in your i3 config and i3 will launch i3bar automatically.\n");
        yyjson_doc_free(doc);
        exit(EXIT_FAILURE);
    }

    /* Parse mode */
    yyjson_val *mode_val = yyjson_obj_get(root, "mode");
    if (mode_val && yyjson_is_str(mode_val)) {
        const char *mode = yyjson_get_str(mode_val);
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
    yyjson_val *hidden_val = yyjson_obj_get(root, "hidden_state");
    if (hidden_val && yyjson_is_str(hidden_val)) {
        const char *hidden = yyjson_get_str(hidden_val);
        DLOG("hidden_state = %s\n", hidden);
        config.hidden_state = (strcmp(hidden, "hide") == 0) ? S_HIDE : S_SHOW;
    }

    /* Parse modifier - can be int or string for backwards compatibility */
    yyjson_val *modifier_val = yyjson_obj_get(root, "modifier");
    if (modifier_val) {
        if (yyjson_is_int(modifier_val)) {
            config.modifier = yyjson_get_int(modifier_val);
            DLOG("modifier = %d\n", config.modifier);
        } else if (yyjson_is_str(modifier_val)) {
            const char *mod = yyjson_get_str(modifier_val);
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
    yyjson_val *position_val = yyjson_obj_get(root, "position");
    if (position_val && yyjson_is_str(position_val)) {
        const char *pos = yyjson_get_str(position_val);
        DLOG("position = %s\n", pos);
        config.position = (strcmp(pos, "top") == 0) ? POS_TOP : POS_BOT;
    }

    /* Parse status_command */
    yyjson_val *status_cmd = yyjson_obj_get(root, "status_command");
    if (status_cmd && yyjson_is_str(status_cmd)) {
        const char *cmd = yyjson_get_str(status_cmd);
        DLOG("status_command = %s\n", cmd);
        config.command = sstrdup(cmd);
    }

    /* Parse workspace_command */
    yyjson_val *ws_cmd = yyjson_obj_get(root, "workspace_command");
    if (ws_cmd && yyjson_is_str(ws_cmd)) {
        const char *cmd = yyjson_get_str(ws_cmd);
        DLOG("workspace_command = %s\n", cmd);
        config.workspace_command = sstrdup(cmd);
    }

    /* Parse font */
    yyjson_val *font_val = yyjson_obj_get(root, "font");
    if (font_val && yyjson_is_str(font_val)) {
        const char *font = yyjson_get_str(font_val);
        DLOG("font = %s\n", font);
        FREE(config.fontname);
        config.fontname = sstrdup(font);
    }

    /* Parse separator_symbol */
    yyjson_val *sep_val = yyjson_obj_get(root, "separator_symbol");
    if (sep_val && yyjson_is_str(sep_val)) {
        const char *sep = yyjson_get_str(sep_val);
        DLOG("separator = %s\n", sep);
        I3STRING_FREE(config.separator_symbol);
        config.separator_symbol = i3string_from_utf8(sep);
    }

    /* Parse bar_height */
    yyjson_val *bar_height = yyjson_obj_get(root, "bar_height");
    if (bar_height && yyjson_is_int(bar_height)) {
        config.bar_height = yyjson_get_int(bar_height);
        DLOG("bar_height = %d\n", config.bar_height);
    }

    /* Parse tray_padding */
    yyjson_val *tray_padding = yyjson_obj_get(root, "tray_padding");
    if (tray_padding && yyjson_is_int(tray_padding)) {
        config.tray_padding = yyjson_get_int(tray_padding);
        DLOG("tray_padding = %d\n", config.tray_padding);
    }

    /* Parse workspace_min_width */
    yyjson_val *ws_min = yyjson_obj_get(root, "workspace_min_width");
    if (ws_min && yyjson_is_int(ws_min)) {
        config.ws_min_width = yyjson_get_int(ws_min);
        DLOG("workspace_min_width = %d\n", config.ws_min_width);
    }

    /* Parse boolean options */
    yyjson_val *binding_mode = yyjson_obj_get(root, "binding_mode_indicator");
    if (binding_mode && yyjson_is_bool(binding_mode)) {
        config.disable_binding_mode_indicator = !yyjson_get_bool(binding_mode);
        DLOG("binding_mode_indicator = %d\n", !config.disable_binding_mode_indicator);
    }

    yyjson_val *ws_buttons = yyjson_obj_get(root, "workspace_buttons");
    if (ws_buttons && yyjson_is_bool(ws_buttons)) {
        config.disable_ws = !yyjson_get_bool(ws_buttons);
        DLOG("workspace_buttons = %d\n", !config.disable_ws);
    }

    yyjson_val *strip_nums = yyjson_obj_get(root, "strip_workspace_numbers");
    if (strip_nums && yyjson_is_bool(strip_nums)) {
        config.strip_ws_numbers = yyjson_get_bool(strip_nums);
        DLOG("strip_workspace_numbers = %d\n", config.strip_ws_numbers);
    }

    yyjson_val *strip_name = yyjson_obj_get(root, "strip_workspace_name");
    if (strip_name && yyjson_is_bool(strip_name)) {
        config.strip_ws_name = yyjson_get_bool(strip_name);
        DLOG("strip_workspace_name = %d\n", config.strip_ws_name);
    }

    yyjson_val *verbose = yyjson_obj_get(root, "verbose");
    if (verbose && yyjson_is_bool(verbose) && !config.verbose) {
        config.verbose = yyjson_get_bool(verbose);
        DLOG("verbose = %d\n", config.verbose);
    }

    /* Parse backwards-compat wheel commands */
    yyjson_val *wheel_up = yyjson_obj_get(root, "wheel_up_cmd");
    if (wheel_up && yyjson_is_str(wheel_up)) {
        DLOG("wheel_up_cmd = %s\n", yyjson_get_str(wheel_up));
        binding_t *binding = scalloc(1, sizeof(binding_t));
        binding->input_code = 4;
        binding->command = sstrdup(yyjson_get_str(wheel_up));
        TAILQ_INSERT_TAIL(&(config.bindings), binding, bindings);
    }

    yyjson_val *wheel_down = yyjson_obj_get(root, "wheel_down_cmd");
    if (wheel_down && yyjson_is_str(wheel_down)) {
        DLOG("wheel_down_cmd = %s\n", yyjson_get_str(wheel_down));
        binding_t *binding = scalloc(1, sizeof(binding_t));
        binding->input_code = 5;
        binding->command = sstrdup(yyjson_get_str(wheel_down));
        TAILQ_INSERT_TAIL(&(config.bindings), binding, bindings);
    }

    /* Parse deprecated single tray_output */
    yyjson_val *tray_output = yyjson_obj_get(root, "tray_output");
    if (tray_output && yyjson_is_str(tray_output)) {
        DLOG("Found deprecated key tray_output %s.\n", yyjson_get_str(tray_output));
        tray_output_t *to = scalloc(1, sizeof(tray_output_t));
        to->output = sstrdup(yyjson_get_str(tray_output));
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
            config.bar_id = sstrdup(yyjson_get_str(first));
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
