/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * mode.c: Handle mode event and show current binding mode in the bar
 *
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

/* Simple JSON access macros - return default if missing or wrong type */
#define json_opt(obj, key, type, def)                                        \
    ({                                                                       \
        yyjson_val *_v = yyjson_obj_get(obj, key);                           \
        (_v && yyjson_is_##type(_v)) ? unsafe_yyjson_get_##type(_v) : (def); \
    })

/*
 * Parse the received JSON string
 *
 */
void parse_mode_json(const unsigned char *json, size_t size) {
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, size, 0, NULL, &err);
    if (!doc) {
        ELOG("JSON parse error for mode event: %s (at position %zu)\n", err.msg, err.pos);
        exit(EXIT_FAILURE);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        ELOG("Could not parse mode event: not an object\n");
        yyjson_doc_free(doc);
        exit(EXIT_FAILURE);
    }

    const char *change = json_opt(root, "change", str, NULL);
    bool pango_markup = json_opt(root, "pango_markup", bool, false);
    DLOG("pango_markup = %d\n", pango_markup);

    mode binding = {0};

    if (change != NULL) {
        binding.name = i3string_from_utf8(change);
        i3string_set_markup(binding.name, pango_markup);
        binding.name_width = predict_text_width(binding.name);
        DLOG("Got mode change: %s\n", i3string_as_utf8(binding.name));
    }

    yyjson_doc_free(doc);

    /* We don't want to indicate default binding mode */
    if (binding.name && strcmp("default", i3string_as_utf8(binding.name)) == 0) {
        I3STRING_FREE(binding.name);
        binding.name = NULL;
    }

    /* Set the new binding mode */
    set_current_mode(&binding);
}
