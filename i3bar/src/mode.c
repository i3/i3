/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * mode.c: Handle mode-event and show current binding mode in the bar
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

#include "common.h"

/* A datatype to pass through the callbacks to save the state */
struct mode_json_params {
    char  *json;
    char  *cur_key;
    mode  *mode;
};

/*
 * Parse a string (change)
 *
 */
#if YAJL_MAJOR >= 2
static int mode_string_cb(void *params_, const unsigned char *val, size_t len) {
#else
static int mode_string_cb(void *params_, const unsigned char *val, unsigned int len) {
#endif
        struct mode_json_params *params = (struct mode_json_params*) params_;

        if (!strcmp(params->cur_key, "change")) {

            /* Save the name */
            params->mode->name = i3string_from_utf8_with_length((const char *)val, len);
            /* Save its rendered width */
            params->mode->width = predict_text_width(params->mode->name);

            DLOG("Got mode change: %s\n", i3string_as_utf8(params->mode->name));
            FREE(params->cur_key);

            return 1;
        }

        return 0;
}

/*
 * Parse a key.
 *
 * Essentially we just save it in the parsing-state
 *
 */
#if YAJL_MAJOR >= 2
static int mode_map_key_cb(void *params_, const unsigned char *keyVal, size_t keyLen) {
#else
static int mode_map_key_cb(void *params_, const unsigned char *keyVal, unsigned int keyLen) {
#endif
    struct mode_json_params *params = (struct mode_json_params*) params_;
    FREE(params->cur_key);

    params->cur_key = smalloc(sizeof(unsigned char) * (keyLen + 1));
    strncpy(params->cur_key, (const char*) keyVal, keyLen);
    params->cur_key[keyLen] = '\0';

    return 1;
}

/* A datastructure to pass all these callbacks to yajl */
yajl_callbacks mode_callbacks = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &mode_string_cb,
    NULL,
    &mode_map_key_cb,
    NULL,
    NULL,
    NULL
};

/*
 * Start parsing the received json-string
 *
 */
void parse_mode_json(char *json) {
    /* FIXME: Fasciliate stream-processing, i.e. allow starting to interpret
     * JSON in chunks */
    struct mode_json_params params;

    mode binding;

    params.cur_key = NULL;
    params.json = json;
    params.mode = &binding;

    yajl_handle handle;
    yajl_status state;

#if YAJL_MAJOR < 2
    yajl_parser_config parse_conf = { 0, 0 };

    handle = yajl_alloc(&mode_callbacks, &parse_conf, NULL, (void*) &params);
#else
    handle = yajl_alloc(&mode_callbacks, NULL, (void*) &params);
#endif

    state = yajl_parse(handle, (const unsigned char*) json, strlen(json));

    /* FIXME: Propper errorhandling for JSON-parsing */
    switch (state) {
        case yajl_status_ok:
            break;
        case yajl_status_client_canceled:
#if YAJL_MAJOR < 2
        case yajl_status_insufficient_data:
#endif
        case yajl_status_error:
            ELOG("Could not parse mode-event!\n");
            exit(EXIT_FAILURE);
            break;
    }

    /* We don't want to indicate default binding mode */
    if (strcmp("default", i3string_as_utf8(params.mode->name)) == 0)
        I3STRING_FREE(params.mode->name);

    /* Set the new binding mode */
    set_current_mode(&binding);

    yajl_free(handle);

    FREE(params.cur_key);
}
