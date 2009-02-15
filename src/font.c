/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
/*
 * Handles font loading
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
i3Font *load_font(xcb_connection_t *c, const char *pattern) {
        /* Check if we got the font cached */
        i3Font *font;
        TAILQ_FOREACH(font, &cached_fonts, fonts)
                if (strcmp(font->pattern, pattern) == 0)
                        return font;

        i3Font *new = malloc(sizeof(i3Font));

        /* Send all our requests first */
        new->id = xcb_generate_id(c);
        xcb_void_cookie_t font_cookie = xcb_open_font_checked(c, new->id, strlen(pattern), pattern);
        xcb_list_fonts_with_info_cookie_t cookie = xcb_list_fonts_with_info(c, 1, strlen(pattern), pattern);

        check_error(c, font_cookie, "Could not open font");

        /* Get information (height/name) for this font */
        xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(c, cookie, NULL);
        if (reply == NULL) {
                printf("Could not load font\n");
                exit(1);
        }

        asprintf(&(new->name), "%.*s", xcb_list_fonts_with_info_name_length(reply),
                        xcb_list_fonts_with_info_name(reply));
        new->pattern = strdup(pattern);
        new->height = reply->font_ascent + reply->font_descent;

        /* Insert into cache */
        TAILQ_INSERT_TAIL(&cached_fonts, new, fonts);

        return new;
}
