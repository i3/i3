/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2015 Ingo Bürk and contributors (see also: LICENSE)
 *
 * cairo_util.h: Utility for operations using cairo.
 *
 */
#pragma once

#include <cairo/cairo-xcb.h>

/* Represents a color split by color channel. */
typedef struct color_t {
    double red;
    double green;
    double blue;
    double alpha;

    /* For compatibility, we also store the colorpixel for now. */
    uint32_t colorpixel;
} color_t;

/* A wrapper grouping an XCB drawable and both a graphics context
 * and the corresponding cairo objects representing it. */
typedef struct surface_t {
    /* The drawable which is being represented. */
    xcb_drawable_t id;

    // TODO remove this once i3 uses solely cairo for drawing operations
    /* A classic XCB graphics context. This should not be used for
     * drawing operations. */
    xcb_gcontext_t gc;

    /* A cairo surface representing the drawable. */
    cairo_surface_t *surface;

    /* The cairo object representing the drawale. In general,
     * this is what one should use for any drawing operation. */
    cairo_t *cr;
} surface_t;

/**
 * Initialize the cairo surface to represent the given drawable.
 *
 */
void cairo_surface_init(surface_t *surface, xcb_drawable_t drawable, int width, int height);

/**
 * Destroys the surface.
 *
 */
void cairo_surface_free(surface_t *surface);

/**
 * Parses the given color in hex format to an internal color representation.
 * Note that the input must begin with a hash sign, e.g., "#3fbc59".
 *
 */
color_t cairo_hex_to_color(const char *color);

/**
 * Set the given color as the source color on the surface.
 *
 */
void cairo_set_source_color(surface_t *surface, color_t color);

/**
 * Draw the given text using libi3.
 * This function also marks the surface dirty which is needed if other means of
 * drawing are used. This will be the case when using XCB to draw text.
 *
 */
void cairo_draw_text(i3String *text, surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width);

/**
 * Draws a filled rectangle.
 * This function is a convenience wrapper and takes care of flushing the
 * surface as well as restoring the cairo state.
 * Note that the drawing is done using CAIRO_OPERATOR_SOURCE.
 *
 */
void cairo_draw_rectangle(surface_t *surface, color_t color, double x, double y, double w, double h);

/**
 * Copies a surface onto another surface.
 * Note that the drawing is done using CAIRO_OPERATOR_SOURCE.
 *
 */
void cairo_copy_surface(surface_t *src, surface_t *dest, double src_x, double src_y,
                        double dest_x, double dest_y, double dest_w, double dest_h);
