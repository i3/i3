/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <xcb/xcb_aux.h>

/**
 * Find the region in the given window that is not covered by a mapped child
 * window.
 */
static cairo_region_t *unobscured_region(xcb_connection_t *conn, xcb_window_t window,
                                         uint16_t window_width, uint16_t window_height) {
    cairo_rectangle_int_t rectangle;
    cairo_region_t *region;

    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = window_width;
    rectangle.height = window_height;
    region = cairo_region_create_rectangle(&rectangle);

    xcb_query_tree_reply_t *tree = xcb_query_tree_reply(conn, xcb_query_tree_unchecked(conn, window), NULL);
    if (!tree) {
        return region;
    }

    /* Get information about children */
    uint16_t n_children = tree->children_len;
    xcb_window_t *children = xcb_query_tree_children(tree);

    xcb_get_geometry_cookie_t geometries[n_children];
    xcb_get_window_attributes_cookie_t attributes[n_children];

    for (int i = 0; i < n_children; i++) {
        geometries[i] = xcb_get_geometry_unchecked(conn, children[i]);
        attributes[i] = xcb_get_window_attributes_unchecked(conn, children[i]);
    }

    /* Remove every visible child from the region */
    for (int i = 0; i < n_children; i++) {
        xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(conn, geometries[i], NULL);
        xcb_get_window_attributes_reply_t *attr = xcb_get_window_attributes_reply(conn, attributes[i], NULL);

        if (geom && attr && attr->map_state == XCB_MAP_STATE_VIEWABLE) {
            rectangle.x = geom->x;
            rectangle.y = geom->y;
            rectangle.width = geom->width;
            rectangle.height = geom->height;
            cairo_region_subtract_rectangle(region, &rectangle);
        }

        free(geom);
        free(attr);
    }

    free(tree);
    return region;
}

static void find_unobscured_pixel(xcb_connection_t *conn, xcb_window_t window,
                                  uint16_t window_width, uint16_t window_height,
                                  uint16_t *x, uint16_t *y) {
    cairo_region_t *region = unobscured_region(conn, window, window_width, window_height);
    if (cairo_region_num_rectangles(region) > 0) {
        /* Return the top left pixel of the first rectangle */
        cairo_rectangle_int_t rect;
        cairo_region_get_rectangle(region, 0, &rect);
        *x = rect.x;
        *y = rect.y;
    } else {
        /* No unobscured area found */
        *x = 0;
        *y = 0;
    }
    cairo_region_destroy(region);
}

static uint32_t flicker_window_at(xcb_connection_t *conn, xcb_screen_t *screen, int16_t x, int16_t y, xcb_window_t window,
                                  uint32_t pixel) {
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, window, screen->root, x, y, 10, 10,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT, (uint32_t[]){pixel, 1});
    xcb_map_window(conn, window);
    xcb_clear_area(conn, 0, window, 0, 0, 0, 0);
    xcb_aux_sync(conn);
    xcb_destroy_window(conn, window);

    xcb_get_image_reply_t *img = xcb_get_image_reply(conn,
                                                     xcb_get_image_unchecked(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->root, x, y, 1, 1, ~0),
                                                     NULL);
    uint32_t result = 0;
    if (img) {
        uint8_t *data = xcb_get_image_data(img);
        uint8_t depth = img->depth;
        for (int i = 0; i < MIN(depth, 4); i++) {
            result = (result << 8) | data[i];
        }
        free(img);
    }
    return result;
}

bool is_background_set(xcb_connection_t *conn, xcb_screen_t *screen) {
    uint16_t x, y;
    find_unobscured_pixel(conn, screen->root, screen->width_in_pixels, screen->height_in_pixels, &x, &y);

    xcb_window_t window = xcb_generate_id(conn);

    uint32_t pixel1 = flicker_window_at(conn, screen, x, y, window, screen->black_pixel);
    uint32_t pixel2 = flicker_window_at(conn, screen, x, y, window, screen->white_pixel);
    return pixel1 == pixel2;
}
