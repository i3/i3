/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * sparse_surface.c: A 'sparse surface' represents the pixmap borders around
 * a window. We allocate four separate pixmaps, one for each side of the
 * window, which means we don't allocate any memory for the contents of
 * the window, which we don't draw to.
 *
 */
#pragma once

#include <config.h>
#include "libi3.h"

/* Enum used to indicate the top, bottom, left, or right side of a
 * sparse surface */
typedef enum {
    SPARSE_SURFACE_TOP = 0,
    SPARSE_SURFACE_BOTTOM = 1,
    SPARSE_SURFACE_LEFT = 2,
    SPARSE_SURFACE_RIGHT = 3,
    SPARSE_SURFACE_SIDE_COUNT = 4
} sparse_surface_side;

/* A wrapper grouping an XCB drawable and both a graphics context
 * and the corresponding cairo objects representing it. This is
 * similar to a surface_t, except that it contains four drawable
 * rectangles, one for each side of the window which allows us to
 * create a 'sparse' pixmap, and save memory by not allocating space
 * for the window contents that we don't draw to. */
typedef struct sparse_surface_t {
    /* The drawables which are being represented. */
    surface_t side_surface[SPARSE_SURFACE_SIDE_COUNT];

    /* x and y offset of each drawable rectangle */
    int x_offset[SPARSE_SURFACE_SIDE_COUNT];
    int y_offset[SPARSE_SURFACE_SIDE_COUNT];

    /* Width and height of the entire sparse surface group */
    int width;
    int height;
} sparse_surface_t;

/**
 * Checks if sparse surface has been initialized (eg, if any of the
 * sparse surface sides are initialized).
 *
 */
bool sparse_surface_initialized(sparse_surface_t *sparse_surface);

/**
 * Initialize the sparse surface to represent the given drawables.
 *
 */
void sparse_surface_create_and_init(xcb_connection_t *conn, sparse_surface_t *sparse_surface,
                                    xcb_drawable_t frame_id,
                                    uint16_t win_depth,
                                    xcb_visualtype_t *visual, int width, int height,
                                    int top_height, int bottom_height, int left_width, int right_width);

/**
 * Destroys the sparse surface.
 *
 */
void sparse_surface_free(xcb_connection_t *conn, sparse_surface_t *sparse_surface);

/**
 * Clears a sparse surface with the given color.
 *
 */
void sparse_surface_clear(sparse_surface_t *sparse_surface, color_t color);

/**
 * Copies a sparse surface onto another non-sparse surface.
 *
 */
void sparse_surface_copy(sparse_surface_t *src_sparse, surface_t *dest, double src_x, double src_y,
                         double dest_x, double dest_y, double width, double height);

/**
 * Draw the given text using libi3.
 *
 */
void sparse_surface_text(i3String *text, sparse_surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width);

/**
 * Draws a filled rectangle.
 *
 */
void sparse_surface_rectangle(sparse_surface_t *surface, color_t color, double x, double y, double w, double h);

/**
 * Draw the given image using libi3.
 * This function is a convenience wrapper and takes care of flushing the
 * surface as well as restoring the cairo state.
 *
 */
void sparse_surface_image(cairo_surface_t *image, sparse_surface_t *surface, int x, int y, int width, int height);

/**
 * Disable GraphicsExposure events
 */
void sparse_surface_disable_graphics_exposure_events(xcb_connection_t *conn, sparse_surface_t *surface);
