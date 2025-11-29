/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * yyjson_utils.h: Utilities and wrappers for yyjson.
 *
 */
#pragma once

#include <yyjson.h>
#include <assert.h>

#include "libi3.h"

/*
 * Custom allocator wrappers for yyjson using i3's allocation functions.
 * smalloc/srealloc abort on allocation failure, so we never return NULL.
 */
static inline void *i3_yyjson_malloc(void *ctx, size_t size) {
    (void)ctx;
    return smalloc(size);
}

static inline void *i3_yyjson_realloc(void *ctx, void *ptr, size_t old_size, size_t size) {
    (void)ctx;
    (void)old_size;
    return srealloc(ptr, size);
}

static inline void i3_yyjson_free(void *ctx, void *ptr) {
    (void)ctx;
    free(ptr);
}

static const yyjson_alc i3_yyjson_alc = {
    .malloc = i3_yyjson_malloc,
    .realloc = i3_yyjson_realloc,
    .free = i3_yyjson_free,
    .ctx = NULL,
};

/*
 * Write a mutable JSON document to a string using i3's allocator.
 * The result must be freed with free().
 *
 * This wrapper:
 * - Always uses i3's allocator (which aborts on OOM)
 * - Asserts that the result is not NULL
 * - Uses default write flags (no pretty printing)
 *
 */
static char *json_write(const yyjson_mut_doc *doc, size_t *len) {
    char *result = yyjson_mut_write_opts(doc, 0, &i3_yyjson_alc, len, NULL);
    assert(result != NULL);
    return result;
}

