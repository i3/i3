/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2015 Ingo Bürk and contributors (see also: LICENSE)
 *
 * draw.h: Utility for drawing.
 *
 */
#pragma once

#ifdef CAIRO_SUPPORT
#include <cairo/cairo-xcb.h>
#endif

#ifdef CAIRO_SUPPORT
/* We need to flush cairo surfaces twice to avoid an assertion bug. See #1989
 * and https://bugs.freedesktop.org/show_bug.cgi?id=92455. */
#define CAIRO_SURFACE_FLUSH(surface)  \
    do {                              \
        cairo_surface_flush(surface); \
        cairo_surface_flush(surface); \
    } while (0)
#endif

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

    /* A classic XCB graphics context. */
    xcb_gcontext_t gc;

    int width;
    int height;

#ifdef CAIRO_SUPPORT
    /* A cairo surface representing the drawable. */
    cairo_surface_t *surface;

    /* The cairo object representing the drawale. In general,
     * this is what one should use for any drawing operation. */
    cairo_t *cr;
#endif
} surface_t;

/**
 * Initialize the surface to represent the given drawable.
 *
 */
void draw_util_surface_init(surface_t *surface, xcb_drawable_t drawable, int width, int height);

/**
 * Destroys the surface.
 *
 */
void draw_util_surface_free(surface_t *surface);

/**
 * Parses the given color in hex format to an internal color representation.
 * Note that the input must begin with a hash sign, e.g., "#3fbc59".
 *
 */
color_t draw_util_hex_to_color(const char *color);

/**
 * Draw the given text using libi3.
 * This function also marks the surface dirty which is needed if other means of
 * drawing are used. This will be the case when using XCB to draw text.
 *
 */
void draw_util_text(i3String *text, surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width);

/**
 * Draws a filled rectangle.
 * This function is a convenience wrapper and takes care of flushing the
 * surface as well as restoring the cairo state.
 *
 */
void draw_util_rectangle(surface_t *surface, color_t color, double x, double y, double w, double h);

/**
 * Clears a surface with the given color.
 *
 */
void draw_util_clear_surface(surface_t *surface, color_t color);

/**
 * Copies a surface onto another surface.
 *
 */
void draw_util_copy_surface(surface_t *src, surface_t *dest, double src_x, double src_y,
                            double dest_x, double dest_y, double width, double height);
