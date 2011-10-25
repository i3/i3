/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ucs2_to_utf8.c: Converts between UCS-2 and UTF-8, both of which are used in
 *                 different contexts in X11.
 */
#ifndef _UCS2_TO_UTF8
#define _UCS2_TO_UTF8

char *convert_utf8_to_ucs2(char *input, int *real_strlen);

#endif
