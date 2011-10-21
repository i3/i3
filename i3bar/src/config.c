/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010-2011 Axel Wagner and contributors
 *
 * See file LICENSE for license information
 *
 * src/outputs.c: Maintaining the output-list
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <i3/ipc.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

#include "common.h"

static char *cur_key;

/*
 * Parse a key.
 *
 * Essentially we just save it in cur_key.
 *
 */
#if YAJL_MAJOR >= 2
static int config_map_key_cb(void *params_, const unsigned char *keyVal, size_t keyLen) {
#else
static int config_map_key_cb(void *params_, const unsigned char *keyVal, unsigned keyLen) {
#endif
    FREE(cur_key);

    cur_key = smalloc(sizeof(unsigned char) * (keyLen + 1));
    strncpy(cur_key, (const char*) keyVal, keyLen);
    cur_key[keyLen] = '\0';

    return 1;
}

/*
 * Parse a string
 *
 */
#if YAJL_MAJOR >= 2
static int config_string_cb(void *params_, const unsigned char *val, size_t len) {
#else
static int config_string_cb(void *params_, const unsigned char *val, unsigned int len) {
#endif
    /* The id is ignored, we already have it in config.bar_id */
    if (!strcmp(cur_key, "id"))
        return 1;

    if (!strcmp(cur_key, "mode")) {
        DLOG("mode = %.*s, len = %d\n", len, val, len);
        config.hide_on_modifier = (len == 4 && !strncmp((const char*)val, "hide", strlen("hide")));
        return 1;
    }

    if (!strcmp(cur_key, "position")) {
        DLOG("position = %.*s\n", len, val);
        config.dockpos = (len == 3 && !strncmp((const char*)val, "top", strlen("top")) ? DOCKPOS_TOP : DOCKPOS_BOT);
        return 1;
    }

    if (!strcmp(cur_key, "status_command")) {
        /* We cannot directly start the child here, because start_child() also
         * needs to be run when no command was specified (to setup stdin).
         * Therefore we save the command in 'config' and access it later in
         * got_bar_config() */
        DLOG("command = %.*s\n", len, val);
        asprintf(&config.command, "%.*s", len, val);
        return 1;
    }

    if (!strcmp(cur_key, "font")) {
        DLOG("font = %.*s\n", len, val);
        asprintf(&config.fontname, "%.*s", len, val);
        return 1;
    }

    if (!strcmp(cur_key, "outputs")) {
        printf("+output %.*s\n", len, val);
        /* XXX: these are not implemented yet */
        return 1;
    }

    if (!strcmp(cur_key, "tray_output")) {
        printf("tray_output %.*s\n", len, val);
        /* XXX: these are not implemented yet */
        return 1;
    }

#define COLOR(json_name, struct_name) \
    do { \
        if (!strcmp(cur_key, #json_name)) { \
            DLOG(#json_name " = " #struct_name " = %.*s\n", len, val); \
            asprintf(&(config.colors.struct_name), "%.*s", len, val); \
            return 1; \
        } \
    } while (0)

    COLOR(statusline, bar_fg);
    COLOR(background, bar_bg);
    COLOR(focused_workspace_text, focus_ws_fg);
    COLOR(focused_workspace_bg, focus_ws_bg);
    COLOR(active_workspace_text, active_ws_fg);
    COLOR(active_workspace_bg, active_ws_bg);
    COLOR(inactive_workspace_text, inactive_ws_fg);
    COLOR(inactive_workspace_bg, inactive_ws_bg);
    COLOR(urgent_workspace_text, urgent_ws_fg);
    COLOR(urgent_workspace_bg, urgent_ws_bg);

    printf("got unexpected string %.*s for cur_key = %s\n", len, val, cur_key);

    return 0;
}

/*
 * Parse a boolean value
 *
 */
static int config_boolean_cb(void *params_, int val) {
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
    NULL,
    &config_boolean_cb,
    NULL,
    NULL,
    NULL,
    &config_string_cb,
    NULL,
    &config_map_key_cb,
    NULL,
    NULL,
    NULL
};

/*
 * Start parsing the received bar configuration json-string
 *
 */
void parse_config_json(char *json) {
    yajl_handle handle;
    yajl_status state;
#if YAJL_MAJOR < 2
    yajl_parser_config parse_conf = { 0, 0 };

    handle = yajl_alloc(&outputs_callbacks, &parse_conf, NULL, NULL);
#else
    handle = yajl_alloc(&outputs_callbacks, NULL, NULL);
#endif

    state = yajl_parse(handle, (const unsigned char*) json, strlen(json));

    /* FIXME: Proper errorhandling for JSON-parsing */
    switch (state) {
        case yajl_status_ok:
            break;
        case yajl_status_client_canceled:
#if YAJL_MAJOR < 2
        case yajl_status_insufficient_data:
#endif
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
    FREE_COLOR(active_ws_fg);
    FREE_COLOR(active_ws_bg);
    FREE_COLOR(inactive_ws_fg);
    FREE_COLOR(inactive_ws_bg);
    FREE_COLOR(urgent_ws_fg);
    FREE_COLOR(urgent_ws_bg);
    FREE_COLOR(focus_ws_fg);
    FREE_COLOR(focus_ws_bg);
#undef FREE_COLOR
}

