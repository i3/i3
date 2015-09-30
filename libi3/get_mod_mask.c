/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include "libi3.h"

extern xcb_connection_t *conn;

/*
 * All-in-one function which returns the modifier mask (XCB_MOD_MASK_*) for the
 * given keysymbol, for example for XCB_NUM_LOCK (usually configured to mod2).
 *
 * This function initiates one round-trip. Use get_mod_mask_for() directly if
 * you already have the modifier mapping and key symbols.
 *
 */
uint32_t aio_get_mod_mask_for(uint32_t keysym, xcb_key_symbols_t *symbols) {
    xcb_get_modifier_mapping_cookie_t cookie;
    xcb_get_modifier_mapping_reply_t *modmap_r;

    xcb_flush(conn);

    /* Get the current modifier mapping (this is blocking!) */
    cookie = xcb_get_modifier_mapping(conn);
    if (!(modmap_r = xcb_get_modifier_mapping_reply(conn, cookie, NULL)))
        return 0;

    uint32_t result = get_mod_mask_for(keysym, symbols, modmap_r);
    free(modmap_r);
    return result;
}

/*
 * Returns the modifier mask (XCB_MOD_MASK_*) for the given keysymbol, for
 * example for XCB_NUM_LOCK (usually configured to mod2).
 *
 * This function does not initiate any round-trips.
 *
 */
uint32_t get_mod_mask_for(uint32_t keysym,
                          xcb_key_symbols_t *symbols,
                          xcb_get_modifier_mapping_reply_t *modmap_reply) {
    xcb_keycode_t *codes, *modmap;
    xcb_keycode_t mod_code;

    modmap = xcb_get_modifier_mapping_keycodes(modmap_reply);

    /* Get the list of keycodes for the given symbol */
    if (!(codes = xcb_key_symbols_get_keycode(symbols, keysym)))
        return 0;

    /* Loop through all modifiers (Mod1-Mod5, Shift, Control, Lock) */
    for (int mod = 0; mod < 8; mod++)
        for (int j = 0; j < modmap_reply->keycodes_per_modifier; j++) {
            /* Store the current keycode (for modifier 'mod') */
            mod_code = modmap[(mod * modmap_reply->keycodes_per_modifier) + j];
            /* Check if that keycode is in the list of previously resolved
             * keycodes for our symbol. If so, return the modifier mask. */
            for (xcb_keycode_t *code = codes; *code; code++) {
                if (*code != mod_code)
                    continue;

                free(codes);
                /* This corresponds to the XCB_MOD_MASK_* constants */
                return (1 << mod);
            }
        }

    return 0;
}
