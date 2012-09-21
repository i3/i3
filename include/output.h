/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * output.c: Output (monitor) related functions.
 *
 */
#ifndef I3_OUTPUT_H
#define I3_OUTPUT_H

/**
 * Returns the output container below the given output container.
 *
 */
Con *output_get_content(Con *output);

#endif
