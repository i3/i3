/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * determine_json_version.c: Determines the JSON protocol version based on the
 *                           first line of input from a child program.
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

static bool version_key;
static int32_t version_number;

#if YAJL_MAJOR >= 2
static int version_integer(void *ctx, long long val) {
#else
static int version_integer(void *ctx, long val) {
#endif
    if (version_key)
        version_number = (uint32_t)val;
    return 1;
}

#if YAJL_MAJOR >= 2
static int version_map_key(void *ctx, const unsigned char *stringval, size_t stringlen) {
#else
static int version_map_key(void *ctx, const unsigned char *stringval, unsigned int stringlen) {
#endif
    version_key = (stringlen == strlen("version") &&
                   strncmp((const char*)stringval, "version", strlen("version")) == 0);
    return 1;
}

static yajl_callbacks version_callbacks = {
    NULL, /* null */
    NULL, /* boolean */
    &version_integer,
    NULL, /* double */
    NULL, /* number */
    NULL, /* string */
    NULL, /* start_map */
    &version_map_key,
    NULL, /* end_map */
    NULL, /* start_array */
    NULL /* end_array */
};

/*
 * Determines the JSON i3bar protocol version from the given buffer. In case
 * the buffer does not contain valid JSON, or no version field is found, this
 * function returns -1. The amount of bytes consumed by parsing the header is
 * returned in *consumed (if non-NULL).
 *
 * The return type is an int32_t to avoid machines with different sizes of
 * 'int' to allow different values here. It’s highly unlikely we ever exceed
 * even an int8_t, but still…
 *
 */
int32_t determine_json_version(const unsigned char *buffer, int length, unsigned int *consumed) {
#if YAJL_MAJOR >= 2
    yajl_handle handle = yajl_alloc(&version_callbacks, NULL, NULL);
    /* Allow trailing garbage. yajl 1 always behaves that way anyways, but for
     * yajl 2, we need to be explicit. */
    yajl_config(handle, yajl_allow_trailing_garbage, 1);
#else
    yajl_parser_config parse_conf = { 0, 0 };

    yajl_handle handle = yajl_alloc(&version_callbacks, &parse_conf, NULL, NULL);
#endif

    version_key = false;
    version_number = -1;

    yajl_status state = yajl_parse(handle, buffer, length);
    if (state != yajl_status_ok) {
        version_number = -1;
        if (consumed != NULL)
            *consumed = 0;
    } else {
        if (consumed != NULL)
            *consumed = yajl_get_bytes_consumed(handle);
    }

    yajl_free(handle);

    return version_number;
}
