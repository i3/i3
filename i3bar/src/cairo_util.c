/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2015 Ingo Bürk and contributors (see also: LICENSE)
 *
 * cairo_util.c: Utility for operations using cairo.
 *
 */
#include <stdlib.h>
#include <err.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <cairo/cairo-xcb.h>

#include "common.h"
#include "libi3.h"

xcb_connection_t *xcb_connection;
xcb_screen_t *root_screen;

/*
 * Initialize the cairo surface to represent the given drawable.
 *
 */
void cairo_surface_init(surface_t *surface, xcb_drawable_t drawable, int width, int height) {
    surface->id = drawable;

    surface->gc = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(xcb_connection, surface->gc, surface->id, 0, NULL);
    if (xcb_request_failed(gc_cookie, "Could not create graphical context"))
        exit(EXIT_FAILURE);

    surface->surface = cairo_xcb_surface_create(xcb_connection, surface->id, get_visualtype(root_screen), width, height);
    surface->cr = cairo_create(surface->surface);
}

/*
 * Destroys the surface.
 *
 */
void cairo_surface_free(surface_t *surface) {
    xcb_free_gc(xcb_connection, surface->gc);
    cairo_surface_destroy(surface->surface);
    cairo_destroy(surface->cr);
}

/*
 * Parses the given color in hex format to an internal color representation.
 * Note that the input must begin with a hash sign, e.g., "#3fbc59".
 *
 */
color_t cairo_hex_to_color(const char *color) {
    char groups[3][3] = {
        {color[1], color[2], '\0'},
        {color[3], color[4], '\0'},
        {color[5], color[6], '\0'}};

    return (color_t){
        .red = strtol(groups[0], NULL, 16) / 255.0,
        .green = strtol(groups[1], NULL, 16) / 255.0,
        .blue = strtol(groups[2], NULL, 16) / 255.0,
        .colorpixel = get_colorpixel(color)};
}

/*
 * Set the given color as the source color on the surface.
 *
 */
void cairo_set_source_color(surface_t *surface, color_t color) {
    cairo_set_source_rgb(surface->cr, color.red, color.green, color.blue);
}
