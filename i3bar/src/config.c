/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2011 Axel Wagner and contributors (see also: LICENSE)
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

#include "common.h"

static char *cur_key;

/*
 * Parse a key.
 *
 * Essentially we just save it in cur_key.
 *
 */
static int config_map_key_cb(void *params_, const unsigned char *keyVal, size_t keyLen) {
    FREE(cur_key);

    cur_key = smalloc(sizeof(unsigned char) * (keyLen + 1));
    strncpy(cur_key, (const char*) keyVal, keyLen);
    cur_key[keyLen] = '\0';

    return 1;
}

/*
 * Parse a null-value (current_workspace)
 *
 */
static int config_null_cb(void *params_) {
    if (!strcmp(cur_key, "id")) {
        /* If 'id' is NULL, the bar config was not found. Error out. */
        ELOG("No such bar config. Use 'i3-msg -t get_bar_config' to get the available configs.\n");
        ELOG("Are you starting i3bar by hand? You should not:\n");
        ELOG("Configure a 'bar' block in your i3 config and i3 will launch i3bar automatically.\n");
        exit(EXIT_FAILURE);
    }

    return 1;
}

/*
 * Parse a string
 *
 */
static int config_string_cb(void *params_, const unsigned char *val, size_t _len) {
    int len = (int)_len;
    /* The id and socket_path are ignored, we already know them. */
    if (!strcmp(cur_key, "id") || !strcmp(cur_key, "socket_path"))
        return 1;

    if (!strcmp(cur_key, "mode")) {
        DLOG("mode = %.*s, len = %d\n", len, val, len);
        config.hide_on_modifier = (len == 4 && !strncmp((const char*)val, "dock", strlen("dock")) ? M_DOCK
            : (len == 4 && !strncmp((const char*)val, "hide", strlen("hide")) ? M_HIDE
                : M_INVISIBLE));
        return 1;
    }

    if (!strcmp(cur_key, "hidden_state")) {
        DLOG("hidden_state = %.*s, len = %d\n", len, val, len);
        config.hidden_state = (len == 4 && !strncmp((const char*)val, "hide", strlen("hide")) ? S_HIDE : S_SHOW);
        return 1;
    }

    if (!strcmp(cur_key, "modifier")) {
        DLOG("modifier = %.*s\n", len, val);
        if (len == 5 && !strncmp((const char*)val, "shift", strlen("shift"))) {
            config.modifier = ShiftMask;
            return 1;
        }
        if (len == 4 && !strncmp((const char*)val, "ctrl", strlen("ctrl"))) {
            config.modifier = ControlMask;
            return 1;
        }
        if (len == 4 && !strncmp((const char*)val, "Mod", strlen("Mod"))) {
            switch (val[3]) {
                case '1':
                    config.modifier = Mod1Mask;
                    return 1;
                case '2':
                    config.modifier = Mod2Mask;
                    return 1;
                case '3':
                    config.modifier = Mod3Mask;
                    return 1;
                /*
                case '4':
                    config.modifier = Mod4Mask;
                    return 1;
                */
                case '5':
                    config.modifier = Mod5Mask;
                    return 1;
            }
        }
        config.modifier = Mod4Mask;
        return 1;
    }

    if (!strcmp(cur_key, "position")) {
        DLOG("position = %.*s\n", len, val);
        config.position = (len == 3 && !strncmp((const char*)val, "top", strlen("top")) ? POS_TOP : POS_BOT);
        return 1;
    }

    if (!strcmp(cur_key, "status_command")) {
        DLOG("command = %.*s\n", len, val);
        sasprintf(&config.command, "%.*s", len, val);
        return 1;
    }

    if (!strcmp(cur_key, "font")) {
        DLOG("font = %.*s\n", len, val);
        sasprintf(&config.fontname, "%.*s", len, val);
        return 1;
    }

    if (!strcmp(cur_key, "outputs")) {
        DLOG("+output %.*s\n", len, val);
        int new_num_outputs = config.num_outputs + 1;
        config.outputs = srealloc(config.outputs, sizeof(char*) * new_num_outputs);
        sasprintf(&config.outputs[config.num_outputs], "%.*s", len, val);
        config.num_outputs = new_num_outputs;
        return 1;
    }

    if (!strcmp(cur_key, "tray_output")) {
        DLOG("tray_output %.*s\n", len, val);
        FREE(config.tray_output);
        sasprintf(&config.tray_output, "%.*s", len, val);
        return 1;
    }

#define COLOR(json_name, struct_name) \
    do { \
        if (!strcmp(cur_key, #json_name)) { \
            DLOG(#json_name " = " #struct_name " = %.*s\n", len, val); \
            sasprintf(&(config.colors.struct_name), "%.*s", len, val); \
            return 1; \
        } \
    } while (0)

    COLOR(statusline, bar_fg);
    COLOR(background, bar_bg);
    COLOR(separator, sep_fg);
    COLOR(focused_workspace_border, focus_ws_border);
    COLOR(focused_workspace_bg, focus_ws_bg);
    COLOR(focused_workspace_text, focus_ws_fg);
    COLOR(active_workspace_border, active_ws_border);
    COLOR(active_workspace_bg, active_ws_bg);
    COLOR(active_workspace_text, active_ws_fg);
    COLOR(inactive_workspace_border, inactive_ws_border);
    COLOR(inactive_workspace_bg, inactive_ws_bg);
    COLOR(inactive_workspace_text, inactive_ws_fg);
    COLOR(urgent_workspace_border, urgent_ws_border);
    COLOR(urgent_workspace_bg, urgent_ws_bg);
    COLOR(urgent_workspace_text, urgent_ws_fg);

    printf("got unexpected string %.*s for cur_key = %s\n", len, val, cur_key);

    return 0;
}

/*
 * Parse a boolean value
 *
 */
static int config_boolean_cb(void *params_, int val) {
    if (!strcmp(cur_key, "binding_mode_indicator")) {
        DLOG("binding_mode_indicator = %d\n", val);
        config.disable_binding_mode_indicator = !val;
        return 1;
    }

    if (!strcmp(cur_key, "workspace_buttons")) {
        DLOG("workspace_buttons = %d\n", val);
        config.disable_ws = !val;
        return 1;
    }

    if (!strcmp(cur_key, "verbose")) {
        DLOG("verbose = %d\n", val);
        config.verbose = val;
        return 1;
    }

    return 0;
}

/* A datastructure to pass all these callbacks to yajl */
static yajl_callbacks outputs_callbacks = {
    .yajl_null = config_null_cb,
    .yajl_boolean = config_boolean_cb,
    .yajl_string = config_string_cb,
    .yajl_map_key = config_map_key_cb,
};

/*
 * Start parsing the received bar configuration json-string
 *
 */
void parse_config_json(char *json) {
    yajl_handle handle;
    yajl_status state;
    handle = yajl_alloc(&outputs_callbacks, NULL, NULL);

    state = yajl_parse(handle, (const unsigned char*) json, strlen(json));

    /* FIXME: Proper errorhandling for JSON-parsing */
    switch (state) {
        case yajl_status_ok:
            break;
        case yajl_status_client_canceled:
        case yajl_status_error:
            ELOG("Could not parse config-reply!\n");
            exit(EXIT_FAILURE);
            break;
    }

    yajl_free(handle);
}

/*
 * free()s the color strings as soon as they are not needed anymore.
 *
 */
void free_colors(struct xcb_color_strings_t *colors) {
#define FREE_COLOR(x) \
    do { \
        if (colors->x) \
            free(colors->x); \
    } while (0)
    FREE_COLOR(bar_fg);
    FREE_COLOR(bar_bg);
    FREE_COLOR(sep_fg);
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
#undef FREE_COLOR
}

