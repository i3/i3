/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2015 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-msg/reformat.h: Reformat the json response from the IPC.
 *
 */
#pragma once

#include <yajl/yajl_gen.h>

yajl_status beautify_json(yajl_gen gen, const unsigned char *data, int len);
