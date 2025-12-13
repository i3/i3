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

/* Simple JSON access macro - return default if missing or wrong type */
#define json_opt(obj, key, type, def)                                        \
    ({                                                                       \
        yyjson_val *_v = yyjson_obj_get(obj, key);                           \
        (_v && yyjson_is_##type(_v)) ? unsafe_yyjson_get_##type(_v) : (def); \
    })

/*
 * Parse the JSON protocol header to determine protocol version and features.
 * In case the buffer does not contain a valid header (invalid JSON, or no
 * version field found), the 'correct' field of the returned header is set to
 * false. The amount of bytes consumed by parsing the header is returned in
 * *consumed (if non-NULL).
 *
 */
void parse_json_header(i3bar_child *child, const unsigned char *buffer, int length, unsigned int *consumed) {
    child->version = 0;
    child->stop_signal = SIGSTOP;
    child->cont_signal = SIGCONT;
    child->click_events = false;

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
        child->version = json_opt(root, "version", int, 0);
        child->stop_signal = json_opt(root, "stop_signal", int, SIGSTOP);
        child->cont_signal = json_opt(root, "cont_signal", int, SIGCONT);
        child->click_events = json_opt(root, "click_events", bool, false);
    }

    if (consumed != NULL) {
        *consumed = yyjson_doc_get_read_size(doc);
    }

    yyjson_doc_free(doc);
}
