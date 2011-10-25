/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>

#include "libi3.h"

extern xcb_connection_t *conn;

/*
 * Loads a font for usage, also getting its height. If fallback is true,
 * the fonts 'fixed' or '-misc-*' will be loaded instead of exiting.
 *
 */
i3Font load_font(const char *pattern, bool fallback) {
    i3Font font;
    xcb_void_cookie_t font_cookie;
    xcb_list_fonts_with_info_cookie_t info_cookie;
    xcb_list_fonts_with_info_reply_t *info_reply;
    xcb_generic_error_t *error;

    /* Send all our requests first */
    font.id = xcb_generate_id(conn);
    font_cookie = xcb_open_font_checked(conn, font.id, strlen(pattern), pattern);
    info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

    /* Check for errors. If errors, fall back to default font. */
    error = xcb_request_check(conn, font_cookie);

    /* If we fail to open font, fall back to 'fixed' */
    if (fallback && error != NULL) {
        ELOG("Could not open font %s (X error %d). Trying fallback to 'fixed'.\n",
             pattern, error->error_code);
        pattern = "fixed";
        font_cookie = xcb_open_font_checked(conn, font.id, strlen(pattern), pattern);
        info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

        /* Check if we managed to open 'fixed' */
        error = xcb_request_check(conn, font_cookie);

        /* Fall back to '-misc-*' if opening 'fixed' fails. */
        if (error != NULL) {
            ELOG("Could not open fallback font 'fixed', trying with '-misc-*'.\n");
            pattern = "-misc-*";
            font_cookie = xcb_open_font_checked(conn, font.id, strlen(pattern), pattern);
            info_cookie = xcb_list_fonts_with_info(conn, 1, strlen(pattern), pattern);

            if ((error = xcb_request_check(conn, font_cookie)) != NULL)
                errx(EXIT_FAILURE, "Could open neither requested font nor fallbacks "
                     "(fixed or -misc-*): X11 error %d", error->error_code);
        }
    }

    /* Get information (height/name) for this font */
    if (!(info_reply = xcb_list_fonts_with_info_reply(conn, info_cookie, NULL)))
        errx(EXIT_FAILURE, "Could not load font \"%s\"", pattern);

    font.height = info_reply->font_ascent + info_reply->font_descent;

    free(info_reply);

    return font;
}
