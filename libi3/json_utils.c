/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * json_utils.c: Utilities and wrappers for yyjson.
 *
 */

#include "libi3.h"

#include <assert.h>
#include <stdlib.h>
#include <yyjson.h>

/*
 * Custom allocator wrappers for yyjson using i3's allocation functions.
 * smalloc/srealloc abort on allocation failure, so we never return NULL.
 */
static void *json_malloc(void *ctx, const size_t size) {
    (void)ctx;
    return smalloc(size);
}

static void *json_realloc(void *ctx, void *ptr, const size_t old_size, const size_t size) {
    (void)ctx;
    (void)old_size;
    return srealloc(ptr, size);
}

static void json_free(void *ctx, void *ptr) {
    (void)ctx;
    free(ptr);
}

static const yyjson_alc json_alc = {
    .malloc = json_malloc,
    .realloc = json_realloc,
    .free = json_free,
    .ctx = NULL,
};

yyjson_mut_doc *json_new(void) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&json_alc);
    assert(doc != NULL); /* Can only be NULL if allocator returns NULL */
    return doc;
}

char *json_write(const yyjson_mut_doc *doc, size_t *len) {
    yyjson_write_err err;
    char *result = yyjson_mut_write_opts(doc, 0, &json_alc, len, &err);
    if (result == NULL) {
        ELOG("yyjson_mut_write failed: code=%u msg=%s\n", err.code, err.msg);
        assert(false);
    }
    return result;
}
