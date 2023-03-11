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
 */

#include "sparse_surface.h"
#include "all.h"

/* The default visual_type to use if none is specified when creating the surface. Must be defined globally. */
extern xcb_visualtype_t *visual_type;

void sparse_surface_free_one_side(xcb_connection_t *conn, sparse_surface_t *sparse_surface, sparse_surface_side side, bool clear_id);
void surface_free(xcb_connection_t *conn,
                  cairo_t **cr,
                  xcb_gcontext_t gc,
                  cairo_surface_t **surface);

bool sparse_surface_initialized(sparse_surface_t *sparse_surface) {
    if (sparse_surface->side_surface[SPARSE_SURFACE_TOP].id == XCB_NONE &&
        sparse_surface->side_surface[SPARSE_SURFACE_BOTTOM].id == XCB_NONE &&
        sparse_surface->side_surface[SPARSE_SURFACE_LEFT].id == XCB_NONE &&
        sparse_surface->side_surface[SPARSE_SURFACE_RIGHT].id == XCB_NONE) {
        ELOG("Surface %p is not initialized, skipping drawing.\n", sparse_surface);
        return false;
    }
    return true;
}

/*
 * Destroys the surface.
 *
 */
void surface_free(xcb_connection_t *conn,
                  cairo_t **cr,
                  xcb_gcontext_t gc,
                  cairo_surface_t **surface) {
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    if (*cr) {
        status = cairo_status(*cr);
    }
    if (status != CAIRO_STATUS_SUCCESS) {
        LOG("Found cairo context in an error status while freeing, error %d is %s",
            status, cairo_status_to_string(status));
    }

    /* NOTE: This function is also called on uninitialised surface_t instances.
     * The x11 error from xcb_free_gc(conn, XCB_NONE) is silently ignored
     * elsewhere.
     */
    xcb_free_gc(conn, gc);
    cairo_surface_destroy(*surface);
    cairo_destroy(*cr);

    /* We need to explicitly set these to NULL to avoid assertion errors in
     * cairo when calling this multiple times. This can happen, for example,
     * when setting the border of a window to none and then closing it. */
    *surface = NULL;
    *cr = NULL;
}

/*
 * Initialize the sparse surface to represent the given drawables.
 *
 */
void sparse_surface_create_and_init(xcb_connection_t *conn, sparse_surface_t *sparse_surface,
                                    xcb_drawable_t frame_id,
                                    uint16_t win_depth,
                                    xcb_visualtype_t *visual, int width, int height,
                                    int top_height, int bottom_height, int left_width, int right_width) {
    int i;

    sparse_surface->width = width;
    sparse_surface->height = height;

    if (visual == NULL)
        visual = visual_type;

    if (top_height == 0 && bottom_height == 0 && left_width == 0 && right_width == 0) {
        /* There is only one pixmap for the top of the window, of size width, height */
        for (i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
            sparse_surface->x_offset[i] = 0;
            sparse_surface->y_offset[i] = 0;
            sparse_surface->side_surface[i].width = 0;
            sparse_surface->side_surface[i].height = 0;
        }

        sparse_surface->side_surface[SPARSE_SURFACE_TOP].width = width;
        sparse_surface->side_surface[SPARSE_SURFACE_TOP].height = height;
        sparse_surface->x_offset[SPARSE_SURFACE_TOP] = 0;
        sparse_surface->y_offset[SPARSE_SURFACE_TOP] = 0;
    } else {
        /* Calculate the size and position of each of the four side pixmaps */
        sparse_surface->side_surface[SPARSE_SURFACE_TOP].width = width - (left_width + right_width);
        sparse_surface->side_surface[SPARSE_SURFACE_TOP].height = top_height;
        sparse_surface->x_offset[SPARSE_SURFACE_TOP] = left_width;
        sparse_surface->y_offset[SPARSE_SURFACE_TOP] = 0;

        sparse_surface->side_surface[SPARSE_SURFACE_BOTTOM].width = width - (left_width + right_width);
        sparse_surface->side_surface[SPARSE_SURFACE_BOTTOM].height = bottom_height;
        sparse_surface->x_offset[SPARSE_SURFACE_BOTTOM] = left_width;
        sparse_surface->y_offset[SPARSE_SURFACE_BOTTOM] = height - bottom_height;

        sparse_surface->side_surface[SPARSE_SURFACE_LEFT].width = left_width;
        sparse_surface->side_surface[SPARSE_SURFACE_LEFT].height = height;
        sparse_surface->x_offset[SPARSE_SURFACE_LEFT] = 0;
        sparse_surface->y_offset[SPARSE_SURFACE_LEFT] = 0;

        sparse_surface->side_surface[SPARSE_SURFACE_RIGHT].width = right_width;
        sparse_surface->side_surface[SPARSE_SURFACE_RIGHT].height = height;
        sparse_surface->x_offset[SPARSE_SURFACE_RIGHT] = width - right_width;
        sparse_surface->y_offset[SPARSE_SURFACE_RIGHT] = 0;
    }

    /* Create each of the four pixmaps */
    for (i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (sparse_surface->side_surface[i].width == 0 || sparse_surface->side_surface[i].height == 0) {
            if (sparse_surface->side_surface[i].id != XCB_NONE) {
                sparse_surface_free_one_side(conn, sparse_surface, i, true);
            }
        } else {
            /* Generate an id if the surface doesn't exist, or remove the surface if it already exists */
            if (sparse_surface->side_surface[i].id != XCB_NONE) {
                sparse_surface_free_one_side(conn, sparse_surface, i, false);
            } else {
                sparse_surface->side_surface[i].id = xcb_generate_id(conn);
            }

            /* create the pixmap for this side */
            xcb_create_pixmap(conn, win_depth, sparse_surface->side_surface[i].id, frame_id,
                              sparse_surface->side_surface[i].width, sparse_surface->side_surface[i].height);

            sparse_surface->side_surface[i].gc = xcb_generate_id(conn);
            xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(conn,
                                                                sparse_surface->side_surface[i].gc,
                                                                sparse_surface->side_surface[i].id,
                                                                0,
                                                                NULL);

            xcb_generic_error_t *error = xcb_request_check(conn, gc_cookie);
            if (error != NULL) {
                ELOG("Could not create graphical context. Error code: %d. Please report this bug.\n", error->error_code);
                free(error);
            }

            sparse_surface->side_surface[i].surface = cairo_xcb_surface_create(conn,
                                                                               sparse_surface->side_surface[i].id,
                                                                               visual,
                                                                               sparse_surface->side_surface[i].width,
                                                                               sparse_surface->side_surface[i].height);

            sparse_surface->side_surface[i].cr = cairo_create(sparse_surface->side_surface[i].surface);

            /* Position this pixmap */
            cairo_translate(sparse_surface->side_surface[i].cr,
                            -(sparse_surface->x_offset[i]),
                            -(sparse_surface->y_offset[i]));
        }
    }
}

/*
 * Destroys the sparse surface.
 *
 */
void sparse_surface_free(xcb_connection_t *conn, sparse_surface_t *sparse_surface) {
    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        sparse_surface_free_one_side(conn, sparse_surface, i, true);
    }
}

/*
 * Destroys one side of the sparse surface.
 *
 */
void sparse_surface_free_one_side(xcb_connection_t *conn, sparse_surface_t *sparse_surface, sparse_surface_side side, bool clear_id) {
    if (sparse_surface->side_surface[side].id != XCB_NONE) {
        surface_free(conn,
                     &(sparse_surface->side_surface[side].cr),
                     sparse_surface->side_surface[side].gc,
                     &(sparse_surface->side_surface[side].surface));

        xcb_free_pixmap(conn, sparse_surface->side_surface[side].id);

        if (clear_id) {
            sparse_surface->side_surface[side].id = XCB_NONE;
        }
    }
}

/*
 * Clears a sparse surface with the given color.
 *
 */
void sparse_surface_clear(sparse_surface_t *sparse_surface, color_t color) {
    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (sparse_surface->side_surface[i].id != XCB_NONE) {
            draw_util_clear_surface(&(sparse_surface->side_surface[i]), color);
        }
    }
}

/*
 * Copies a sparse surface onto another non-sparse surface.
 *
 */
void sparse_surface_copy(sparse_surface_t *src_sparse, surface_t *dest, double src_x, double src_y,
                         double dest_x, double dest_y, double width, double height) {
    if (!sparse_surface_initialized(src_sparse) ||
        dest->id == XCB_NONE) {
        return;
    }

    double src_x_adjusted;
    double src_y_adjusted;

    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (src_sparse->side_surface[i].id != XCB_NONE) {
            cairo_save(dest->cr);

            /* src_x and src_y are relative to the entire window+border area.
             * src_x_adjusted and src_y_adjusted are relative to an individual
             * border pixmap (there are 4 per container). */
            src_x_adjusted = src_x - src_sparse->x_offset[i];
            src_y_adjusted = src_y - src_sparse->y_offset[i];

            /* Using the SOURCE operator will copy both color and alpha information directly
             * onto the surface rather than blending it. This is a bit more efficient and
             * allows better color control for the user when using opacity. */
            cairo_set_operator(dest->cr, CAIRO_OPERATOR_SOURCE);

            cairo_set_source_surface(dest->cr, src_sparse->side_surface[i].surface, dest_x - src_x_adjusted, dest_y - src_y_adjusted);

            cairo_rectangle(dest->cr, dest_x - src_x_adjusted, dest_y - src_y_adjusted,
                            src_sparse->side_surface[i].width, src_sparse->side_surface[i].height);
            cairo_fill(dest->cr);

            /* Make sure we flush the surface for any text drawing operations that could follow.
             * Since we support drawing text via XCB, we need this. */
            CAIRO_SURFACE_FLUSH(src_sparse->side_surface[i].surface);
            CAIRO_SURFACE_FLUSH(dest->surface);

            cairo_restore(dest->cr);
        }
    }
}

/**
 * Draw the given text using libi3.
 *
 */
void sparse_surface_text(i3String *text, sparse_surface_t *surface, color_t fg_color, color_t bg_color, int x, int y, int max_width) {
    /* Draw to each of the four border pixmaps. This ensures that any draws that cross pixmap boundaries are
     * drawn correctly. */
    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (surface->side_surface[i].id != XCB_NONE) {
            draw_util_text(text, &(surface->side_surface[i]), fg_color, bg_color, x, y, max_width);
        }
    }
}

/**
 * Draws a filled rectangle.
 *
 */
void sparse_surface_rectangle(sparse_surface_t *surface, color_t color, double x, double y, double w, double h) {
    /* Draw to each of the four border pixmaps. This ensures that any draws that cross pixmap boundaries are
     * drawn correctly. */
    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (surface->side_surface[i].id != XCB_NONE) {
            draw_util_rectangle(&(surface->side_surface[i]), color, x, y, w, h);
        }
    }
}

/**
 * Draw the given image using libi3.
 * This function is a convenience wrapper and takes care of flushing the
 * surface as well as restoring the cairo state.
 *
 */
void sparse_surface_image(cairo_surface_t *image, sparse_surface_t *surface, int x, int y, int width, int height) {
    /* Draw to each of the four border pixmaps. This ensures that any draws that cross pixmap boundaries are
     * drawn correctly. */
    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (surface->side_surface[i].id != XCB_NONE) {
            draw_util_image(image, &(surface->side_surface[i]), x, y, width, height);
        }
    }
}

/**
 * Disable GraphicsExposure events
 */
void sparse_surface_disable_graphics_exposure_events(xcb_connection_t *conn, sparse_surface_t *surface) {
    for (int i = 0; i < SPARSE_SURFACE_SIDE_COUNT; i++) {
        if (surface->side_surface[i].id != XCB_NONE) {
            xcb_change_gc(conn, surface->side_surface[i].gc, XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){0});
        }
    }
}