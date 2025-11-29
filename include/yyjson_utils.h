/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * yyjson_utils.h: Utility functions and macros for JSON generation with yyjson.
 *
 */
#pragma once

#include <config.h>
#include <yyjson.h>
#include <string.h>

/*
 * Helper macros for yyjson JSON generation.
 * These provide a similar interface to the old yajl macros but use yyjson.
 */

/* Create a new mutable document */
#define YYJSON_DOC_NEW() yyjson_mut_doc_new(NULL)

/* Free a mutable document */
#define YYJSON_DOC_FREE(doc) yyjson_mut_doc_free(doc)

/* Write document to string. Caller must free the returned string. */
static inline char *yyjson_mut_write_str(yyjson_mut_doc *doc, size_t *len) {
    return yyjson_mut_write(doc, 0, len);
}

/* Helper to add a string key-value pair to an object */
static inline void yyjson_obj_add_str(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                      const char *key, const char *val) {
    yyjson_mut_obj_add_strcpy(doc, obj, key, val);
}

/* Helper to add a string key with null value to an object */
static inline void yyjson_obj_add_null(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                       const char *key) {
    yyjson_mut_obj_add_null(doc, obj, key);
}

/* Helper to add an integer key-value pair to an object */
static inline void yyjson_obj_add_int(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                      const char *key, int64_t val) {
    yyjson_mut_obj_add_int(doc, obj, key, val);
}

/* Helper to add an unsigned integer key-value pair to an object */
static inline void yyjson_obj_add_uint(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                       const char *key, uint64_t val) {
    yyjson_mut_obj_add_uint(doc, obj, key, val);
}

/* Helper to add a boolean key-value pair to an object */
static inline void yyjson_obj_add_bool(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                       const char *key, bool val) {
    yyjson_mut_obj_add_bool(doc, obj, key, val);
}

/* Helper to add a double key-value pair to an object */
static inline void yyjson_obj_add_real(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                       const char *key, double val) {
    yyjson_mut_obj_add_real(doc, obj, key, val);
}

/* Helper to add an object key-value pair to an object */
static inline yyjson_mut_val *yyjson_obj_add_obj(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                                  const char *key) {
    yyjson_mut_val *new_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, obj, key, new_obj);
    return new_obj;
}

/* Helper to add an array key-value pair to an object */
static inline yyjson_mut_val *yyjson_obj_add_arr(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                                                  const char *key) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, obj, key, arr);
    return arr;
}

/* Helper to append a string to an array */
static inline void yyjson_arr_add_str(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                      const char *val) {
    yyjson_mut_arr_add_strcpy(doc, arr, val);
}

/* Helper to append an integer to an array */
static inline void yyjson_arr_add_int(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                      int64_t val) {
    yyjson_mut_arr_add_int(doc, arr, val);
}

/* Helper to append an object to an array and return it */
static inline yyjson_mut_val *yyjson_arr_add_obj(yyjson_mut_doc *doc, yyjson_mut_val *arr) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_arr_add_val(arr, obj);
    return obj;
}

