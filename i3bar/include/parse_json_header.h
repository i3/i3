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
#pragma once

#include <stdint.h>

/**
 * Parse the JSON protocol header to determine protocol version and features.
 * In case the buffer does not contain a valid header (invalid JSON, or no
 * version field found), the 'correct' field of the returned header is set to
 * false. The amount of bytes consumed by parsing the header is returned in
 * *consumed (if non-NULL).
 *
 */
void parse_json_header(i3bar_child *child, const unsigned char *buffer, int length, unsigned int *consumed);
