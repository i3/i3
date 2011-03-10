/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * xcb.c: Helper functions for easier usage of XCB
 *
 */

#include "all.h"

TAILQ_HEAD(cached_fonts_head, Font) cached_fonts = TAILQ_HEAD_INITIALIZER(cached_fonts);
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
    if (error != NULL) {
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

    return new;
}

/*
 * Returns the colorpixel to use for the given hex color (think of HTML).
 *
 * The hex_color has to start with #, for example #FF00FF.
 *
 * NOTE that get_colorpixel() does _NOT_ check the given color code for validity.
 * This has to be done by the caller.
 *
 */
uint32_t get_colorpixel(char *hex) {
    char strgroups[3][3] = {{hex[1], hex[2], '\0'},
                            {hex[3], hex[4], '\0'},
                            {hex[5], hex[6], '\0'}};
    uint32_t rgb16[3] = {(strtol(strgroups[0], NULL, 16)),
                         (strtol(strgroups[1], NULL, 16)),
                         (strtol(strgroups[2], NULL, 16))};

    return (rgb16[0] << 16) + (rgb16[1] << 8) + rgb16[2];
}

/*
 * Convenience wrapper around xcb_create_window which takes care of depth, generating an ID and checking
 * for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect dims, uint16_t window_class,
        enum xcursor_cursor_t cursor, bool map, uint32_t mask, uint32_t *values) {
    xcb_window_t result = xcb_generate_id(conn);
    xcb_cursor_t cursor_id = xcb_generate_id(conn);

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
 * Changes a single value in the graphic context (so one doesn’t have to define an array of values)
 *
 */
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value) {
    xcb_change_gc(conn, gc, mask, &value);
}

/*
 * Draws a line from x,y to to_x,to_y using the given color
 *
 */
void xcb_draw_line(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t to_x, uint32_t to_y) {
    xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, colorpixel);
    xcb_point_t points[] = {{x, y}, {to_x, to_y}};
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, drawable, gc, 2, points);
}

/*
 * Draws a rectangle from x,y with width,height using the given color
 *
 */
void xcb_draw_rect(xcb_connection_t *conn, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, colorpixel);
    xcb_rectangle_t rect = {x, y, width, height};
    xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);
}

/*
 * Generates a configure_notify event and sends it to the given window
 * Applications need this to think they’ve configured themselves correctly.
 * The truth is, however, that we will manage them.
 *
 */
void fake_configure_notify(xcb_connection_t *conn, Rect r, xcb_window_t window) {
    xcb_configure_notify_event_t generated_event;

    generated_event.event = window;
    generated_event.window = window;
    generated_event.response_type = XCB_CONFIGURE_NOTIFY;

    generated_event.x = r.x;
    generated_event.y = r.y;
    generated_event.width = r.width;
    generated_event.height = r.height;

    generated_event.border_width = 0;
    generated_event.above_sibling = XCB_NONE;
    generated_event.override_redirect = false;

    xcb_send_event(conn, false, window, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*)&generated_event);
    xcb_flush(conn);
}

/*
 * Generates a configure_notify_event with absolute coordinates (relative to the X root
 * window, not to the client’s frame) for the given client.
 *
 */
void fake_absolute_configure_notify(Con *con) {
    Rect absolute;
    if (con->window == NULL)
        return;

    absolute.x = con->rect.x + con->window_rect.x;
    absolute.y = con->rect.y + con->window_rect.y;
    absolute.width = con->window_rect.width;
    absolute.height = con->window_rect.height;

    DLOG("fake rect = (%d, %d, %d, %d)\n", absolute.x, absolute.y, absolute.width, absolute.height);

    fake_configure_notify(conn, absolute, con->window->id);
}

/*
 * Finds out which modifier mask is the one for numlock, as the user may change this.
 *
 */
void xcb_get_numlock_mask(xcb_connection_t *conn) {
    xcb_key_symbols_t *keysyms;
    xcb_get_modifier_mapping_cookie_t cookie;
    xcb_get_modifier_mapping_reply_t *reply;
    xcb_keycode_t *modmap;
    int mask, i;
    const int masks[8] = { XCB_MOD_MASK_SHIFT,
                           XCB_MOD_MASK_LOCK,
                           XCB_MOD_MASK_CONTROL,
                           XCB_MOD_MASK_1,
                           XCB_MOD_MASK_2,
                           XCB_MOD_MASK_3,
                           XCB_MOD_MASK_4,
                           XCB_MOD_MASK_5 };

    /* Request the modifier map */
    cookie = xcb_get_modifier_mapping_unchecked(conn);

    /* Get the keysymbols */
    keysyms = xcb_key_symbols_alloc(conn);

    if ((reply = xcb_get_modifier_mapping_reply(conn, cookie, NULL)) == NULL) {
        xcb_key_symbols_free(keysyms);
        return;
    }

    modmap = xcb_get_modifier_mapping_keycodes(reply);

    /* Get the keycode for numlock */
#ifdef OLD_XCB_KEYSYMS_API
    xcb_keycode_t numlock = xcb_key_symbols_get_keycode(keysyms, XCB_NUM_LOCK);
#else
    /* For now, we only use the first keysymbol. */
    xcb_keycode_t *numlock_syms = xcb_key_symbols_get_keycode(keysyms, XCB_NUM_LOCK);
    if (numlock_syms == NULL)
        return;
    xcb_keycode_t numlock = *numlock_syms;
    free(numlock_syms);
#endif

    /* Check all modifiers (Mod1-Mod5, Shift, Control, Lock) */
    for (mask = 0; mask < 8; mask++)
        for (i = 0; i < reply->keycodes_per_modifier; i++)
            if (modmap[(mask * reply->keycodes_per_modifier) + i] == numlock)
                xcb_numlock_mask = masks[mask];

    xcb_key_symbols_free(keysyms);
    free(reply);
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
 *
 * Prepares the given Cached_Pixmap for usage (checks whether the size of the
 * object this pixmap is related to (e.g. a window) has changed and re-creates
 * the pixmap if so).
 *
 */
void cached_pixmap_prepare(xcb_connection_t *conn, struct Cached_Pixmap *pixmap) {
    DLOG("preparing pixmap\n");

    /* If the Rect did not change, the pixmap does not need to be recreated */
    if (memcmp(&(pixmap->rect), pixmap->referred_rect, sizeof(Rect)) == 0)
        return;

    memcpy(&(pixmap->rect), pixmap->referred_rect, sizeof(Rect));

    if (pixmap->id == 0 || pixmap->gc == 0) {
        DLOG("Creating new pixmap...\n");
        pixmap->id = xcb_generate_id(conn);
        pixmap->gc = xcb_generate_id(conn);
    } else {
        DLOG("Re-creating this pixmap...\n");
        xcb_free_gc(conn, pixmap->gc);
        xcb_free_pixmap(conn, pixmap->id);
    }

    xcb_create_pixmap(conn, root_depth, pixmap->id,
                      pixmap->referred_drawable, pixmap->rect.width, pixmap->rect.height);

    xcb_create_gc(conn, pixmap->gc, pixmap->id, 0, 0);
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
    add_ignore_event(cookie.sequence);
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
