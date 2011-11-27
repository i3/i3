/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>

#include "libi3.h"

extern xcb_connection_t *conn;
static const i3Font *savedFont = NULL;

/*
 * Loads a font for usage, also getting its metrics. If fallback is true,
 * the fonts 'fixed' or '-misc-*' will be loaded instead of exiting.
 *
 */
i3Font load_font(const char *pattern, const bool fallback) {
    i3Font font;

    /* Send all our requests first */
    font.id = xcb_generate_id(conn);
    xcb_void_cookie_t font_cookie = xcb_open_font_checked(conn, font.id,
            strlen(pattern), pattern);
    xcb_query_font_cookie_t info_cookie = xcb_query_font(conn, font.id);

    /* Check for errors. If errors, fall back to default font. */
    xcb_generic_error_t *error;
    error = xcb_request_check(conn, font_cookie);

    /* If we fail to open font, fall back to 'fixed' */
    if (fallback && error != NULL) {
        ELOG("Could not open font %s (X error %d). Trying fallback to 'fixed'.\n",
             pattern, error->error_code);
        pattern = "fixed";
        font_cookie = xcb_open_font_checked(conn, font.id, strlen(pattern), pattern);
        info_cookie = xcb_query_font(conn, font.id);

        /* Check if we managed to open 'fixed' */
        error = xcb_request_check(conn, font_cookie);

        /* Fall back to '-misc-*' if opening 'fixed' fails. */
        if (error != NULL) {
            ELOG("Could not open fallback font 'fixed', trying with '-misc-*'.\n");
            pattern = "-misc-*";
            font_cookie = xcb_open_font_checked(conn, font.id, strlen(pattern), pattern);
            info_cookie = xcb_query_font(conn, font.id);

            if ((error = xcb_request_check(conn, font_cookie)) != NULL)
                errx(EXIT_FAILURE, "Could open neither requested font nor fallbacks "
                     "(fixed or -misc-*): X11 error %d", error->error_code);
        }
    }

    /* Get information (height/name) for this font */
    if (!(font.info = xcb_query_font_reply(conn, info_cookie, NULL)))
        errx(EXIT_FAILURE, "Could not load font \"%s\"", pattern);

    /* Get the font table, if possible */
    if (xcb_query_font_char_infos_length(font.info) == 0)
        font.table = NULL;
    else
        font.table = xcb_query_font_char_infos(font.info);

    /* Calculate the font height */
    font.height = font.info->font_ascent + font.info->font_descent;

    return font;
}

/*
 * Defines the font to be used for the forthcoming calls.
 *
 */
void set_font(i3Font *font) {
    savedFont = font;
}

/*
 * Frees the resources taken by the current font.
 *
 */
void free_font() {
    /* Close the font and free the info */
    xcb_close_font(conn, savedFont->id);
    if (savedFont->info)
        free(savedFont->info);
}

/*
 * Defines the colors to be used for the forthcoming draw_text calls.
 *
 */
void set_font_colors(xcb_gcontext_t gc, uint32_t foreground, uint32_t background) {
    assert(savedFont != NULL);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t values[] = { foreground, background, savedFont->id };
    xcb_change_gc(conn, gc, mask, values);
}

/*
 * Draws text onto the specified X drawable (normally a pixmap) at the
 * specified coordinates (from the top left corner of the leftmost, uppermost
 * glyph) and using the provided gc. Text can be specified as UCS-2 or UTF-8.
 *
 */
void draw_text(char *text, size_t text_len, bool is_ucs2, xcb_drawable_t drawable,
               xcb_gcontext_t gc, int x, int y, int max_width) {
    assert(savedFont != NULL);
    assert(text_len != 0);

    /* X11 coordinates for fonts start at the baseline */
    int pos_y = y + savedFont->info->font_ascent;

    /* As an optimization, check if we can bypass conversion */
    if (!is_ucs2 && text_len <= 255) {
        xcb_image_text_8(conn, text_len, drawable, gc, x, pos_y, text);
        return;
    }

    /* Convert the text into UCS-2 so we can do basic pointer math */
    char *input = (is_ucs2 ? text : (char*)convert_utf8_to_ucs2(text, &text_len));

    /* The X11 protocol limits text drawing to 255 chars, so we may need
     * multiple calls */
    int pos_x = x;
    int offset = 0;
    for (;;) {
        /* Calculate the size of this chunk */
        int chunk_size = (text_len > 255 ? 255 : text_len);
        xcb_char2b_t *chunk = (xcb_char2b_t*)input + offset;

        /* Draw it */
        xcb_image_text_16(conn, chunk_size, drawable, gc, pos_x, pos_y, chunk);

        /* Advance the offset and length of the text to draw */
        offset += chunk_size;
        text_len -= chunk_size;

        /* Check if we're done */
        if (text_len == 0)
            break;

        /* Advance pos_x based on the predicted text width */
        pos_x += predict_text_width((char*)chunk, chunk_size, true);
    }

    /* If we had to convert, free the converted string */
    if (!is_ucs2)
        free(input);
}

static int xcb_query_text_width(xcb_char2b_t *text, size_t text_len) {
    /* Make the user know we’re using the slow path, but only once. */
    static bool first_invocation = true;
    if (first_invocation) {
        fprintf(stderr, "Using slow code path for text extents\n");
        first_invocation = false;
    }

    /* Query the text width */
    xcb_generic_error_t *error;
    xcb_query_text_extents_cookie_t cookie = xcb_query_text_extents(conn,
            savedFont->id, text_len, (xcb_char2b_t*)text);
    xcb_query_text_extents_reply_t *reply = xcb_query_text_extents_reply(conn,
            cookie, &error);
    if (reply == NULL) {
        /* We return a safe estimate because a rendering error is better than
         * a crash. Plus, the user will see the error in his log. */
        fprintf(stderr, "Could not get text extents (X error code %d)\n",
                error->error_code);
        return savedFont->info->max_bounds.character_width * text_len;
    }

    int width = reply->overall_width;
    free(reply);
    return width;
}

/*
 * Predict the text width in pixels for the given text. Text can be specified
 * as UCS-2 or UTF-8.
 *
 */
int predict_text_width(char *text, size_t text_len, bool is_ucs2) {
    /* Convert the text into UTF-16 so we can do basic pointer math */
    xcb_char2b_t *input;
    if (is_ucs2)
        input = (xcb_char2b_t*)text;
    else
        input = convert_utf8_to_ucs2(text, &text_len);

    int width;
    if (savedFont->table == NULL) {
        /* If we don't have a font table, fall back to querying the server */
        width = xcb_query_text_width(input, text_len);
    } else {
        /* Save some pointers for convenience */
        xcb_query_font_reply_t *font_info = savedFont->info;
        xcb_charinfo_t *font_table = savedFont->table;

        /* Calculate the width using the font table */
        width = 0;
        for (size_t i = 0; i < text_len; i++) {
            xcb_charinfo_t *info;
            int row = input[i].byte1;
            int col = input[i].byte2;

            if (row < font_info->min_byte1 ||
                row > font_info->max_byte1 ||
                col < font_info->min_char_or_byte2 ||
                col > font_info->max_char_or_byte2)
                continue;

            /* Don't you ask me, how this one works… (Merovius) */
            info = &font_table[((row - font_info->min_byte1) *
                    (font_info->max_char_or_byte2 - font_info->min_char_or_byte2 + 1)) +
                (col - font_info->min_char_or_byte2)];

            if (info->character_width != 0 ||
                    (info->right_side_bearing |
                     info->left_side_bearing |
                     info->ascent |
                     info->descent) != 0) {
                width += info->character_width;
            }
        }
    }

    /* If we had to convert, free the converted string */
    if (!is_ucs2)
        free(input);

    return width;
}
