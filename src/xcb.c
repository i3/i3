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
 * Loads a font for usage, also getting its height. If fallback is true,
 * i3 loads 'fixed' or '-misc-*' if the font cannot be found instead of
 * exiting.
 *
 */
i3Font load_font(const char *pattern, bool fallback) {
    i3Font new;
    xcb_void_cookie_t font_cookie;
    xcb_list_fonts_with_info_cookie_t info_cookie;

    /* Send all our requests first */
    new.id = xcb_generate_id(conn);
    font_cookie = xcb_open_font_checked(conn, new.id, strlen(pattern), pattern);
    info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

    /* Check for errors. If errors, fall back to default font. */
    xcb_generic_error_t *error = xcb_request_check(conn, font_cookie);

    /* If we fail to open font, fall back to 'fixed'. If opening 'fixed' fails fall back to '-misc-*' */
    if (fallback && error != NULL) {
        ELOG("Could not open font %s (X error %d). Reverting to backup font.\n", pattern, error->error_code);
        pattern = "fixed";
        font_cookie = xcb_open_font_checked(conn, new.id, strlen(pattern), pattern);
        info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

        /* Check if we managed to open 'fixed' */
        xcb_generic_error_t *error = xcb_request_check(conn, font_cookie);

        /* Fall back to '-misc-*' if opening 'fixed' fails. */
        if (error != NULL) {
            ELOG("Could not open fallback font '%s', trying with '-misc-*'\n",pattern);
            pattern = "-misc-*";
            font_cookie = xcb_open_font_checked(conn, new.id, strlen(pattern), pattern);
            info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

            check_error(conn, font_cookie, "Could open neither requested font nor fallback (fixed or -misc-*");
        }
    }

    /* Get information (height/name) for this font */
    xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(conn, info_cookie, NULL);
    exit_if_null(reply, "Could not load font \"%s\"\n", pattern);

    new.height = reply->font_ascent + reply->font_descent;

    free(reply);

    return new;
}

/*
 * Convenience wrapper around xcb_create_window which takes care of depth, generating an ID and checking
 * for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect dims, uint16_t window_class,
        enum xcursor_cursor_t cursor, bool map, uint32_t mask, uint32_t *values) {
    xcb_window_t result = xcb_generate_id(conn);

    /* If the window class is XCB_WINDOW_CLASS_INPUT_ONLY, depth has to be 0 */
    uint16_t depth = (window_class == XCB_WINDOW_CLASS_INPUT_ONLY ? 0 : XCB_COPY_FROM_PARENT);

    xcb_create_window(conn,
            depth,
            result, /* the window id */
            root, /* parent == root */
            dims.x, dims.y, dims.width, dims.height, /* dimensions */
            0, /* border = 0, we draw our own */
            window_class,
            XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
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
        xcb_create_glyph_cursor(conn, cursor_id, cursor_font.id, cursor_font.id,
                xcb_cursor, xcb_cursor + 1, 0, 0, 0, 65535, 65535, 65535);
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
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]){ colorpixel });
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2,
                  (xcb_point_t[]) { {x, y}, {to_x, to_y} });
}

/*
 * Draws a rectangle from x,y with width,height using the given color
 *
 */
void xcb_draw_rect(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, (uint32_t[]){ colorpixel });
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
void send_take_focus(xcb_window_t window) {
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
    ev->data.data32[1] = XCB_CURRENT_TIME;

    DLOG("Sending WM_TAKE_FOCUS to the client\n");
    xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char*)ev);
    free(event);
}

/*
 * Raises the given window (typically client->frame) above all other windows
 *
 */
void xcb_raise_window(xcb_connection_t *conn, xcb_window_t window) {
    uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(conn, window, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

/*
 * Query the width of the given text (16-bit characters, UCS) with given real
 * length (amount of glyphs) using the given font.
 *
 */
int predict_text_width(char *text, int length) {
    xcb_query_text_extents_cookie_t cookie;
    xcb_query_text_extents_reply_t *reply;
    xcb_generic_error_t *error;
    int width;

    cookie = xcb_query_text_extents(conn, config.font.id, length, (xcb_char2b_t*)text);
    if ((reply = xcb_query_text_extents_reply(conn, cookie, &error)) == NULL) {
        ELOG("Could not get text extents (X error code %d)\n",
             error->error_code);
        /* We return the rather safe guess of 7 pixels, because a
         * rendering error is better than a crash. Plus, the user will
         * see the error in his log. */
        return 7;
    }

    width = reply->overall_width;
    free(reply);
    return width;
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
    xcb_create_glyph_cursor(conn, cursor_id, cursor_font.id, cursor_font.id,
            xcb_cursor, xcb_cursor + 1, 0, 0, 0, 65535, 65535, 65535);
    xcb_change_window_attributes(conn, root, XCB_CW_CURSOR, &cursor_id);
    xcb_free_cursor(conn, cursor_id);
    xcb_flush(conn);
}
