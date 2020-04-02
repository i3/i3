/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * mode.c: Handle "layer" events and show current layer in the bar
 *
 */
#pragma once

#include <config.h>

#include <xcb/xproto.h>

#include "common.h"

/* Name of current binding mode and its render width */
struct layer {
  i3String *name;
  int name_width;
  long long from;
  long long to;
};

typedef struct layer layer;

/*
 * Start parsing the received JSON string
 *
 */
void parse_layer_json(char *json);
