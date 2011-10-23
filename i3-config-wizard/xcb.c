/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <X11/keysym.h>

#include "xcb.h"

extern xcb_window_t root;
unsigned int xcb_numlock_mask;

/*
 * Convenience-wrapper around xcb_change_gc which saves us declaring a variable
 *
 */
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value) {
        xcb_change_gc(conn, gc, mask, &value);
}

/*
 * Returns the mask for Mode_switch (to be used for looking up keysymbols by
 * keycode).
 *
 */
uint32_t get_mod_mask(xcb_connection_t *conn, uint32_t keycode) {
	xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(conn);

	xcb_get_modifier_mapping_reply_t *modmap_r;
	xcb_keycode_t *modmap, kc;
	xcb_keycode_t *modeswitchcodes = xcb_key_symbols_get_keycode(symbols, keycode);
	if (modeswitchcodes == NULL)
		return 0;

	modmap_r = xcb_get_modifier_mapping_reply(conn, xcb_get_modifier_mapping(conn), NULL);
	modmap = xcb_get_modifier_mapping_keycodes(modmap_r);

	for (int i = 0; i < 8; i++)
		for (int j = 0; j < modmap_r->keycodes_per_modifier; j++) {
			kc = modmap[i * modmap_r->keycodes_per_modifier + j];
			for (xcb_keycode_t *ktest = modeswitchcodes; *ktest; ktest++) {
				if (*ktest != kc)
					continue;

				free(modeswitchcodes);
				free(modmap_r);
				return (1 << i);
			}
		}

	return 0;
}

/*
 * Opens the window we use for input/output and maps it
 *
 */
xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height) {
        xcb_window_t win = xcb_generate_id(conn);
        //xcb_cursor_t cursor_id = xcb_generate_id(conn);

#if 0
        /* Use the default cursor (left pointer) */
        if (cursor > -1) {
                i3Font *cursor_font = load_font(conn, "cursor");
                xcb_create_glyph_cursor(conn, cursor_id, cursor_font->id, cursor_font->id,
                                XCB_CURSOR_LEFT_PTR, XCB_CURSOR_LEFT_PTR + 1,
                                0, 0, 0, 65535, 65535, 65535);
        }
#endif

        uint32_t mask = 0;
        uint32_t values[3];

        mask |= XCB_CW_BACK_PIXEL;
        values[0] = 0;

	mask |= XCB_CW_EVENT_MASK;
	values[1] = XCB_EVENT_MASK_EXPOSURE |
                    XCB_EVENT_MASK_BUTTON_PRESS;

        xcb_create_window(conn,
                          XCB_COPY_FROM_PARENT,
                          win, /* the window id */
                          root, /* parent == root */
                          490, 297, width, height, /* dimensions */
                          0, /* border = 0, we draw our own */
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                          mask,
                          values);

#if 0
        if (cursor > -1)
                xcb_change_window_attributes(conn, result, XCB_CW_CURSOR, &cursor_id);
#endif

        /* Map the window (= make it visible) */
        xcb_map_window(conn, win);

	return win;
}

/*
 * Returns the ID of the font matching the given pattern and stores the height
 * of the font (in pixels) in *font_height. die()s if no font matches.
 *
 */
int get_font_id(xcb_connection_t *conn, char *pattern, int *font_height) {
        xcb_void_cookie_t font_cookie;
        xcb_list_fonts_with_info_cookie_t info_cookie;

        /* Send all our requests first */
        int result;
        result = xcb_generate_id(conn);
        font_cookie = xcb_open_font_checked(conn, result, strlen(pattern), pattern);
        info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

        xcb_generic_error_t *error = xcb_request_check(conn, font_cookie);
        if (error != NULL) {
                fprintf(stderr, "ERROR: Could not open font: %d\n", error->error_code);
                exit(1);
        }

        /* Get information (height/name) for this font */
        xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(conn, info_cookie, NULL);
        if (reply == NULL)
                errx(1, "Could not load font \"%s\"\n", pattern);

        *font_height = reply->font_ascent + reply->font_descent;

        return result;
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

