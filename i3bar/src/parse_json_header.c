/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * parse_json_header.c: Parse the JSON protocol header to determine
 *                      protocol version and features.
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <ev.h>
#include <stdbool.h>
#include <stdint.h>
#include <yajl/yajl_common.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

#include "common.h"

static enum {
    KEY_VERSION,
    KEY_STOP_SIGNAL,
    KEY_CONT_SIGNAL,
    KEY_CLICK_EVENTS,
    NO_KEY
} current_key;

static int header_integer(void *ctx, long long val) {
    i3bar_child *child = ctx;

    switch (current_key) {
        case KEY_VERSION:
            child->version = val;
            break;
        case KEY_STOP_SIGNAL:
            child->stop_signal = val;
            break;
        case KEY_CONT_SIGNAL:
            child->cont_signal = val;
            break;
        default:
            break;
    }

    return 1;
}

static int header_boolean(void *ctx, int val) {
    i3bar_child *child = ctx;

    switch (current_key) {
        case KEY_CLICK_EVENTS:
            child->click_events = val;
            break;
        default:
            break;
    }

    return 1;
}

#define CHECK_KEY(name) (stringlen == strlen(name) && \
                         STARTS_WITH((const char *)stringval, stringlen, name))

static int header_map_key(void *ctx, const unsigned char *stringval, size_t stringlen) {
    if (CHECK_KEY("version")) {
        current_key = KEY_VERSION;
    } else if (CHECK_KEY("stop_signal")) {
        current_key = KEY_STOP_SIGNAL;
    } else if (CHECK_KEY("cont_signal")) {
        current_key = KEY_CONT_SIGNAL;
    } else if (CHECK_KEY("click_events")) {
        current_key = KEY_CLICK_EVENTS;
    }
    return 1;
}

static void child_init(i3bar_child *child) {
    child->version = 0;
    child->stop_signal = SIGSTOP;
    child->cont_signal = SIGCONT;
}

/*
 * Parse the JSON protocol header to determine protocol version and features.
 * In case the buffer does not contain a valid header (invalid JSON, or no
 * version field found), the 'correct' field of the returned header is set to
 * false. The amount of bytes consumed by parsing the header is returned in
 * *consumed (if non-NULL).
 *
 */
void parse_json_header(i3bar_child *child, const unsigned char *buffer, int length, unsigned int *consumed) {
    static yajl_callbacks version_callbacks = {
        .yajl_boolean = header_boolean,
        .yajl_integer = header_integer,
        .yajl_map_key = &header_map_key,
    };

    child_init(child);

    current_key = NO_KEY;

    yajl_handle handle = yajl_alloc(&version_callbacks, NULL, child);
    /* Allow trailing garbage. yajl 1 always behaves that way anyways, but for
     * yajl 2, we need to be explicit. */
    yajl_config(handle, yajl_allow_trailing_garbage, 1);

    yajl_status state = yajl_parse(handle, buffer, length);
    if (state != yajl_status_ok) {
        child_init(child);
        if (consumed != NULL)
            *consumed = 0;
    } else {
        if (consumed != NULL)
            *consumed = yajl_get_bytes_consumed(handle);
    }

    yajl_free(handle);
}
