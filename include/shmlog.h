/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * The format of the shmlog data structure which i3 development versions use by
 * default (ringbuffer for storing the debug log).
 *
 */
#ifndef _I3_SHMLOG_H
#define _I3_SHMLOG_H

#include <stdint.h>

typedef struct i3_shmlog_header {
    uint32_t offset_next_write;
    uint32_t offset_last_wrap;
    uint32_t size;
} i3_shmlog_header;

#endif
