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
#include "all.h"

#define FOR_EACH_SURFACE(multi_surface, index, surf, offset_x, offset_y) \
    for ((index) = 0, (surf) = &((multi_surface)->surfaces[0].surface), (offset_x) = (multi_surface)->surfaces[0].x, (offset_y) = (multi_surface)->surfaces[0].y; \
            (index) < 4 && (multi_surface)->surfaces[index].surface.surface != NULL; \
            (index)++, (surf) = &((multi_surface)->surfaces[(index)].surface), (offset_x) = (multi_surface)->surfaces[(index)].x, (offset_y) = (multi_surface)->surfaces[(index)].y)

/**
 * Initialize the multi surface to draw to the given drawables.
 *
 */
void multi_draw_init(xcb_connection_t *conn, multi_surface_t *surface, xcb_visualtype_t *visual,
                     int num_drawables, xcb_drawable_t *drawables, int *xs, int *ys, int *widths, int *heights) {
    assert(num_drawables <= sizeof(surface->surfaces) / sizeof(surface->surfaces[0]));

    /* Set everything to NULL; this is used to indicate not-present surfaces */
    memset(surface, 0, sizeof(*surface));

    for (int i = 0; i < num_drawables; i++) {
        surface->surfaces[i].x = xs[i];
        surface->surfaces[i].y = ys[i];
        draw_util_surface_init(conn, &(surface->surfaces[i].surface), drawables[i], visual, widths[i], heights[i]);
    }
}

/**
 * Destroys the multi surface.
 *
 */
void multi_surface_free(multi_surface_t *surface) {
    surface_t *s;
    int index, offset_x, offset_y;
    xcb_connection_t *conn = cairo_xcb_device_get_connection(cairo_surface_get_device(surface->surfaces[0].surface.surface));
    FOR_EACH_SURFACE(surface, index, s, offset_x, offset_y) {
        draw_util_surface_free(conn, s);
        (void) offset_x;
        (void) offset_y;
    }

    /* Set everything to NULL; this is used to indicate not-present surfaces */
    memset(surface, 0, sizeof(*surface));
}

/**
 * Destroys the multi surface and calls xcb_free_pixmap() on each drawable.
 *
 */
void multi_surface_free_pixmap(multi_surface_t *surface) {
    xcb_pixmap_t pixmaps[4];
    xcb_connection_t *conn = cairo_xcb_device_get_connection(cairo_surface_get_device(surface->surfaces[0].surface.surface));

    /* Check that we got the array size correct */
    assert(sizeof(pixmaps) / sizeof(pixmaps[0]) == sizeof(surface->surfaces) / sizeof(surface->surfaces[0]));
    for (int i = 0; i < sizeof(pixmaps) / sizeof(pixmaps[0]); i++)
        pixmaps[i] = surface->surfaces[i].surface.id;

    multi_surface_free(surface);
    for (int i = 0; i < sizeof(pixmaps) / sizeof(pixmaps[0]); i++) {
        if (pixmaps[i] != XCB_NONE)
            xcb_free_pixmap(conn, pixmaps[i]);
    }
}

/**
 * Draw the given text using libi3.
 *
 */
void multi_surface_text(i3String *text, multi_surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width) {
    surface_t *s;
    int index, offset_x, offset_y;
    FOR_EACH_SURFACE(surface, index, s, offset_x, offset_y) {
        draw_util_text(text, s, fg_color, bg_color, x - offset_x, y - offset_y, max_width);
    }
}

/**
 * Draws a filled rectangle.
 *
 */
void multi_surface_rectangle(multi_surface_t *surface, color_t color, double x, double y, double w, double h) {
    surface_t *s;
    int index, offset_x, offset_y;
    FOR_EACH_SURFACE(surface, index, s, offset_x, offset_y) {
        draw_util_rectangle(s, color, x - offset_x, y - offset_y, w, h);
    }
}

/**
 * Clears a surface with the given color.
 *
 */
void multi_surface_clear(multi_surface_t *surface, color_t color) {
    surface_t *s;
    int index, offset_x, offset_y;
    FOR_EACH_SURFACE(surface, index, s, offset_x, offset_y) {
        draw_util_clear_surface(s, color);
        (void) offset_x;
        (void) offset_y;
    }
}

/**
 * Copies a multi surface onto a "normal" surface. Areas not covered by the
 * multi surface are filled with fill_color.
 *
 */
void multi_surface_copy_surface(multi_surface_t *src, surface_t *dest, int src_x, int src_y,
                                int dest_x, int dest_y, unsigned int width, unsigned int height, color_t fill_color) {
    surface_t *s;
    int index, offset_x, offset_y;

    /* Figure out which area is not covered by any of the sub-surface */
    cairo_region_t *region = cairo_region_create_rectangle(&(cairo_rectangle_int_t){dest_x, dest_y, width, height});
    FOR_EACH_SURFACE(src, index, s, offset_x, offset_y) {
        cairo_region_subtract_rectangle(region, &(cairo_rectangle_int_t){offset_x, offset_y, s->width, s->height});
    }

    /* Fill the uncovered rectangles */
    for (int i = 0; i < cairo_region_num_rectangles(region); i++) {
        cairo_rectangle_int_t rect;
        cairo_region_get_rectangle(region, i, &rect);
        draw_util_rectangle(dest, fill_color, rect.x, rect.y, rect.width, rect.height);
    }

    /* Copy everything else */
    FOR_EACH_SURFACE(src, index, s, offset_x, offset_y) {
        draw_util_copy_surface(s, dest, src_x - offset_x, src_y - offset_y, dest_x, dest_y, width, height);
    }

    cairo_region_destroy(region);
}
