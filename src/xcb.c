#undef I3__FILE__
#define I3__FILE__ "xcb.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * xcb.c: Helper functions for easier usage of XCB
 *
 */
#include "all.h"

unsigned int xcb_numlock_mask;

/*
 * Convenience wrapper around xcb_create_window which takes care of depth, generating an ID and checking
 * for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect dims,
                           uint16_t depth, xcb_visualid_t visual, uint16_t window_class,
                           enum xcursor_cursor_t cursor, bool map, uint32_t mask, uint32_t *values) {
    xcb_window_t result = xcb_generate_id(conn);

    /* If the window class is XCB_WINDOW_CLASS_INPUT_ONLY, we copy depth and
     * visual id from the parent window. */
    if (window_class == XCB_WINDOW_CLASS_INPUT_ONLY) {
        depth = XCB_COPY_FROM_PARENT;
        visual = XCB_COPY_FROM_PARENT;
    }

    xcb_create_window(conn,
                      depth,
                      result,                                  /* the window id */
                      root,                                    /* parent == root */
                      dims.x, dims.y, dims.width, dims.height, /* dimensions */
                      0,                                       /* border = 0, we draw our own */
                      window_class,
                      visual,
                      mask,
                      values);

    /* Set the cursor */
    if (xcursor_supported) {
        mask = XCB_CW_CURSOR;
        values[0] = xcursor_get_cursor(cursor);
        xcb_change_window_attributes(conn, result, mask, values);
    } else {
        xcb_cursor_t cursor_id = xcb_generate_id(conn);
        i3Font cursor_font = load_font("cursor", false);
        int xcb_cursor = xcursor_get_xcb_cursor(cursor);
        xcb_create_glyph_cursor(conn, cursor_id, cursor_font.specific.xcb.id,
                                cursor_font.specific.xcb.id, xcb_cursor, xcb_cursor + 1, 0, 0, 0,
                                65535, 65535, 65535);
        xcb_change_window_attributes(conn, result, XCB_CW_CURSOR, &cursor_id);
        xcb_free_cursor(conn, cursor_id);
    }

    /* Map the window (= make it visible) */
    if (map)
        xcb_map_window(conn, result);

    return result;
}

/*
 * Draws a line from x,y to to_x,to_y using the given color
 *
 */
void xcb_draw_line(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t to_x, uint32_t to_y) {
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]) {colorpixel});
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2,
                  (xcb_point_t[]) {{x, y}, {to_x, to_y}});
}

/*
 * Draws a rectangle from x,y with width,height using the given color
 *
 */
void xcb_draw_rect(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]) {colorpixel});
    xcb_rectangle_t rect = {x, y, width, height};
    xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);
}

/*
 * Generates a configure_notify_event with absolute coordinates (relative to the X root
 * window, not to the client’s frame) for the given client.
 *
 */
void fake_absolute_configure_notify(Con *con) {
    xcb_rectangle_t absolute;
    if (con->window == NULL)
        return;

    absolute.x = con->rect.x + con->window_rect.x;
    absolute.y = con->rect.y + con->window_rect.y;
    absolute.width = con->window_rect.width;
    absolute.height = con->window_rect.height;

    DLOG("fake rect = (%d, %d, %d, %d)\n", absolute.x, absolute.y, absolute.width, absolute.height);

    fake_configure_notify(conn, absolute, con->window->id, con->border_width);
}

/*
 * Sends the WM_TAKE_FOCUS ClientMessage to the given window
 *
 */
void send_take_focus(xcb_window_t window, xcb_timestamp_t timestamp) {
    /* Every X11 event is 32 bytes long. Therefore, XCB will copy 32 bytes.
     * In order to properly initialize these bytes, we allocate 32 bytes even
     * though we only need less for an xcb_configure_notify_event_t */
    void *event = scalloc(32);
    xcb_client_message_event_t *ev = event;

    ev->response_type = XCB_CLIENT_MESSAGE;
    ev->window = window;
    ev->type = A_WM_PROTOCOLS;
    ev->format = 32;
    ev->data.data32[0] = A_WM_TAKE_FOCUS;
    ev->data.data32[1] = timestamp;

    DLOG("Sending WM_TAKE_FOCUS to the client\n");
    xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char *)ev);
    free(event);
}

/*
 * Raises the given window (typically client->frame) above all other windows
 *
 */
void xcb_raise_window(xcb_connection_t *conn, xcb_window_t window) {
    uint32_t values[] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(conn, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

/*
 * Configures the given window to have the size/position specified by given rect
 *
 */
void xcb_set_window_rect(xcb_connection_t *conn, xcb_window_t window, Rect r) {
    xcb_void_cookie_t cookie;
    cookie = xcb_configure_window(conn, window,
                                  XCB_CONFIG_WINDOW_X |
                                      XCB_CONFIG_WINDOW_Y |
                                      XCB_CONFIG_WINDOW_WIDTH |
                                      XCB_CONFIG_WINDOW_HEIGHT,
                                  &(r.x));
    /* ignore events which are generated because we configured a window */
    add_ignore_event(cookie.sequence, -1);
}

/*
 * Returns true if the given reply contains the given atom.
 *
 */
bool xcb_reply_contains_atom(xcb_get_property_reply_t *prop, xcb_atom_t atom) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0)
        return false;

    xcb_atom_t *atoms;
    if ((atoms = xcb_get_property_value(prop)) == NULL)
        return false;

    for (int i = 0; i < xcb_get_property_value_length(prop) / (prop->format / 8); i++)
        if (atoms[i] == atom)
            return true;

    return false;
}

/**
 * Moves the mouse pointer into the middle of rect.
 *
 */
void xcb_warp_pointer_rect(xcb_connection_t *conn, Rect *rect) {
    int mid_x = rect->x + (rect->width / 2);
    int mid_y = rect->y + (rect->height / 2);

    LOG("warp pointer to: %d %d\n", mid_x, mid_y);
    xcb_warp_pointer(conn, XCB_NONE, root, 0, 0, 0, 0, mid_x, mid_y);
}

/*
 * Set the cursor of the root window to the given cursor id.
 * This function should only be used if xcursor_supported == false.
 * Otherwise, use xcursor_set_root_cursor().
 *
 */
void xcb_set_root_cursor(int cursor) {
    xcb_cursor_t cursor_id = xcb_generate_id(conn);
    i3Font cursor_font = load_font("cursor", false);
    int xcb_cursor = xcursor_get_xcb_cursor(cursor);
    xcb_create_glyph_cursor(conn, cursor_id, cursor_font.specific.xcb.id,
                            cursor_font.specific.xcb.id, xcb_cursor, xcb_cursor + 1, 0, 0, 0,
                            65535, 65535, 65535);
    xcb_change_window_attributes(conn, root, XCB_CW_CURSOR, &cursor_id);
    xcb_free_cursor(conn, cursor_id);
    xcb_flush(conn);
}

/*
 * Get depth of visual specified by visualid
 *
 */
uint16_t get_visual_depth(xcb_visualid_t visual_id) {
    xcb_depth_iterator_t depth_iter;

    depth_iter = xcb_screen_allowed_depths_iterator(root_screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_visualtype_iterator_t visual_iter;

        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
            if (visual_id == visual_iter.data->visual_id) {
                return depth_iter.data->depth;
            }
        }
    }
    return 0;
}

/*
 * Get visualid with specified depth
 *
 */
xcb_visualid_t get_visualid_by_depth(uint16_t depth) {
    xcb_depth_iterator_t depth_iter;

    depth_iter = xcb_screen_allowed_depths_iterator(root_screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        if (depth_iter.data->depth != depth)
            continue;

        xcb_visualtype_iterator_t visual_iter;

        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        if (!visual_iter.rem)
            continue;
        return visual_iter.data->visual_id;
    }
    return 0;
}
