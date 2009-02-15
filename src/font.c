/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * font.c: Handles font loading (with caching, with height information)
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>

#include "data.h"
#include "util.h"

TAILQ_HEAD(cached_fonts_head, Font) cached_fonts = TAILQ_HEAD_INITIALIZER(cached_fonts);

/*
 * Loads a font for usage, getting its height. This function is used very often, so it
 * maintains a cache.
 *
 */
i3Font *load_font(xcb_connection_t *connection, const char *pattern) {
        /* Check if we got the font cached */
        i3Font *font;
        TAILQ_FOREACH(font, &cached_fonts, fonts)
                if (strcmp(font->pattern, pattern) == 0)
                        return font;

        i3Font *new = smalloc(sizeof(i3Font));
        xcb_void_cookie_t font_cookie;
        xcb_list_fonts_with_info_cookie_t info_cookie;

        /* Send all our requests first */
        new->id = xcb_generate_id(connection);
        font_cookie = xcb_open_font_checked(connection, new->id, strlen(pattern), pattern);
        info_cookie = xcb_list_fonts_with_info(connection, 1, strlen(pattern), pattern);

        check_error(connection, font_cookie, "Could not open font");

        /* Get information (height/name) for this font */
        xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(connection, info_cookie, NULL);
        exit_if_null(reply, "Could not load font \"%s\"\n", pattern);

        if (asprintf(&(new->name), "%.*s", xcb_list_fonts_with_info_name_length(reply),
                                           xcb_list_fonts_with_info_name(reply)) == -1)
                die("asprintf() failed\n");
        new->pattern = sstrdup(pattern);
        new->height = reply->font_ascent + reply->font_descent;

        /* Insert into cache */
        TAILQ_INSERT_TAIL(&cached_fonts, new, fonts);

        return new;
}
