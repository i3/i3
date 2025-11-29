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
#include "common.h"

#include <signal.h>
#include <string.h>

#include <yyjson.h>

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
    child_init(child);

    /* YYJSON_READ_STOP_WHEN_DONE allows trailing content after the JSON object */
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)buffer, length,
                                       YYJSON_READ_STOP_WHEN_DONE, NULL, &err);

    if (!doc) {
        if (consumed != NULL) {
            *consumed = 0;
        }
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);

    if (yyjson_is_obj(root)) {
        yyjson_val *version_val = yyjson_obj_get(root, "version");
        if (version_val && yyjson_is_int(version_val)) {
            child->version = yyjson_get_int(version_val);
        }

        yyjson_val *stop_signal_val = yyjson_obj_get(root, "stop_signal");
        if (stop_signal_val && yyjson_is_int(stop_signal_val)) {
            child->stop_signal = yyjson_get_int(stop_signal_val);
        }

        yyjson_val *cont_signal_val = yyjson_obj_get(root, "cont_signal");
        if (cont_signal_val && yyjson_is_int(cont_signal_val)) {
            child->cont_signal = yyjson_get_int(cont_signal_val);
        }

        yyjson_val *click_events_val = yyjson_obj_get(root, "click_events");
        if (click_events_val && yyjson_is_bool(click_events_val)) {
            child->click_events = yyjson_get_bool(click_events_val);
        }
    }

    if (consumed != NULL) {
        *consumed = yyjson_doc_get_read_size(doc);
    }

    yyjson_doc_free(doc);
}
