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
