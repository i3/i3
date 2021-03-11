/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2021 Uli Schlachter and contributors (see also: LICENSE)
 *
 * Support for "drawing with holes": Think of a window with a frame around it.
 * There is some drawing on each side and in the middle a big hole where the
 * inner window is.
 *
 */
#pragma once

#include <config.h>
#include "libi3.h"

/* A wrapper around multiple surface_t arranged in a big, virtual layout. */
typedef struct multi_surface_t {
    struct {
        surface_t surface;
        int x;
        int y;
    } surfaces[4];
} multi_surface_t;

/**
 * Initialize the multi surface to draw to the given drawables.
 *
 */
void multi_draw_init(xcb_connection_t *conn, multi_surface_t *surface, xcb_visualtype_t *visual,
                     int num_drawables, xcb_drawable_t *drawables, int *xs, int *ys, int *widths, int *heights);

/**
 * Destroys the multi surface.
 *
 */
void multi_surface_free(multi_surface_t *surface);

/**
 * Draw the given text using libi3.
 *
 */
void multi_surface_text(i3String *text, multi_surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width);

/**
 * Draws a filled rectangle.
 *
 */
void multi_surface_rectangle(multi_surface_t *surface, color_t color, double x, double y, double w, double h);

/**
 * Clears a surface with the given color.
 *
 */
void multi_surface_clear(multi_surface_t *surface, color_t color);

/**
 * Copies a multi surface onto a "normal" surface. Areas not covered by the
 * multi surface are filled with fill_color.
 *
 */
void multi_surface_copy_surface(multi_surface_t *src, surface_t *dest, int src_x, int src_y,
                                int dest_x, int dest_y, unsigned int width, unsigned int height, color_t fill_color);
