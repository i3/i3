/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * xcb.c: Helper functions for easier usage of XCB
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>

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

/*
 * Returns the colorpixel to use for the given hex color (think of HTML).
 *
 * The hex_color has to start with #, for example #FF00FF.
 *
 * The client argument is optional. If it is given, the colorpixel will be cached.
 *
 * NOTE that get_colorpixel() does _NOT_ check the given color code for validity.
 * This has to be done by the caller.
 *
 */
uint32_t get_colorpixel(xcb_connection_t *conn, Client *client, xcb_window_t window, char *hex) {
        /* Lookup this colorpixel in the cache if a client was specified */
        if (client != NULL) {
                struct Colorpixel *pixel;
                SLIST_FOREACH(pixel, &(client->colorpixels), colorpixels)
                        if (strcmp(pixel->hex, hex) == 0)
                                return pixel->pixel;
        }
        #define RGB_8_TO_16(i) (65535 * ((i) & 0xFF) / 255)
        char strgroups[3][3] = {{hex[1], hex[2], '\0'},
                                {hex[3], hex[4], '\0'},
                                {hex[5], hex[6], '\0'}};
        int rgb16[3] = {RGB_8_TO_16(strtol(strgroups[0], NULL, 16)),
                        RGB_8_TO_16(strtol(strgroups[1], NULL, 16)),
                        RGB_8_TO_16(strtol(strgroups[2], NULL, 16))};

        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        xcb_colormap_t colormap_id = xcb_generate_id(conn);
        xcb_void_cookie_t cookie = xcb_create_colormap_checked(conn, XCB_COLORMAP_ALLOC_NONE,
                                   colormap_id, window, root_screen->root_visual);
        check_error(conn, cookie, "Could not create colormap");
        xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(conn,
                        xcb_alloc_color(conn, colormap_id, rgb16[0], rgb16[1], rgb16[2]), NULL);

        if (!reply) {
                printf("Could not allocate color\n");
                exit(1);
        }

        uint32_t pixel = reply->pixel;
        free(reply);
        xcb_free_colormap(conn, colormap_id);

        /* Store the result in the cache if a client was specified */
        if (client != NULL) {
                struct Colorpixel *cache_pixel = scalloc(sizeof(struct Colorpixel));
                cache_pixel->hex = sstrdup(hex);
                cache_pixel->pixel = pixel;

                SLIST_INSERT_HEAD(&(client->colorpixels), cache_pixel, colorpixels);
        }

        return pixel;
}

/*
 * Convenience wrapper around xcb_create_window which takes care of depth, generating an ID and checking
 * for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect dims, uint16_t window_class, uint32_t mask, uint32_t *values) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        xcb_window_t result = xcb_generate_id(conn);
        xcb_void_cookie_t cookie;

        /* If the window class is XCB_WINDOW_CLASS_INPUT_ONLY, depth has to be 0 */
        uint16_t depth = (window_class == XCB_WINDOW_CLASS_INPUT_ONLY ? 0 : XCB_COPY_FROM_PARENT);

        cookie = xcb_create_window_checked(conn,
                                           depth,
                                           result, /* the window id */
                                           root, /* parent == root */
                                           dims.x, dims.y, dims.width, dims.height, /* dimensions */
                                           0, /* border = 0, we draw our own */
                                           window_class,
                                           XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                                           mask,
                                           values);
        check_error(conn, cookie, "Could not create window");

        /* Map the window (= make it visible) */
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
void xcb_draw_rect(xcb_connection_t *connection, xcb_drawable_t drawable, xcb_gcontext_t gc,
                   uint32_t colorpixel, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        xcb_change_gc_single(connection, gc, XCB_GC_FOREGROUND, colorpixel);
        xcb_rectangle_t rect = {x, y, width, height};
        xcb_poly_fill_rectangle(connection, drawable, gc, 1, &rect);
}
