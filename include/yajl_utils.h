/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * yajl_utils.h
 *
 */
#ifndef I3_YAJL_UTILS_H
#define I3_YAJL_UTILS_H

#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

/* Shorter names for all those yajl_gen_* functions */
#define y(x, ...) yajl_gen_ ## x (gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(gen, (unsigned char*)str, strlen(str))

#if YAJL_MAJOR >= 2
#define ygenalloc() yajl_gen_alloc(NULL)
#define yalloc(callbacks, client) yajl_alloc(callbacks, NULL, client)
typedef size_t ylength;
#else
#define ygenalloc() yajl_gen_alloc(NULL, NULL);
#define yalloc(callbacks, client) yajl_alloc(callbacks, NULL, NULL, client)
typedef unsigned int ylength;
#endif

#endif
