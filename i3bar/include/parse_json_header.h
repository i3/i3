/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * parse_json_header.c: Parse the JSON protocol header to determine
 *                      protocol version and features.
 *
 */
#ifndef PARSE_JSON_HEADER_H_
#define PARSE_JSON_HEADER_H_

#include <stdint.h>

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
void parse_json_header(i3bar_child *child, const unsigned char *buffer, int length, unsigned int *consumed);

#endif
