/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <err.h>
#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>

#include "libi3.h"

static iconv_t utf8_conversion_descriptor = (iconv_t)-1;
static iconv_t ucs2_conversion_descriptor = (iconv_t)-1;

/*
 * Converts the given string to UTF-8 from UCS-2 big endian. The return value
 * must be freed after use.
 *
 */
char *convert_ucs2_to_utf8(xcb_char2b_t *text, size_t num_glyphs) {
    /* Allocate the output buffer (UTF-8 is at most 4 bytes per glyph) */
    size_t buffer_size = num_glyphs * 4 * sizeof(char) + 1;
    char *buffer = scalloc(buffer_size);

    /* We need to use an additional pointer, because iconv() modifies it */
    char *output = buffer;
    size_t output_size = buffer_size - 1;

    if (utf8_conversion_descriptor == (iconv_t)-1) {
        /* Get a new conversion descriptor */
        utf8_conversion_descriptor = iconv_open("UTF-8", "UCS-2BE");
        if (utf8_conversion_descriptor == (iconv_t)-1)
            err(EXIT_FAILURE, "Error opening the conversion context");
    } else {
        /* Reset the existing conversion descriptor */
        iconv(utf8_conversion_descriptor, NULL, NULL, NULL, NULL);
    }

    /* Do the conversion */
    size_t input_len = num_glyphs * sizeof(xcb_char2b_t);
    size_t rc = iconv(utf8_conversion_descriptor, (char **)&text,
                      &input_len, &output, &output_size);
    if (rc == (size_t)-1) {
        perror("Converting to UTF-8 failed");
        free(buffer);
        return NULL;
    }

    return buffer;
}

/*
 * Converts the given string to UCS-2 big endian for use with
 * xcb_image_text_16(). The amount of real glyphs is stored in real_strlen,
 * a buffer containing the UCS-2 encoded string (16 bit per glyph) is
 * returned. It has to be freed when done.
 *
 */
xcb_char2b_t *convert_utf8_to_ucs2(char *input, size_t *real_strlen) {
    /* Calculate the input buffer size (UTF-8 is strlen-safe) */
    size_t input_size = strlen(input);

    /* Calculate the output buffer size and allocate the buffer */
    size_t buffer_size = input_size * sizeof(xcb_char2b_t);
    xcb_char2b_t *buffer = smalloc(buffer_size);

    /* We need to use an additional pointer, because iconv() modifies it */
    size_t output_size = buffer_size;
    xcb_char2b_t *output = buffer;

    if (ucs2_conversion_descriptor == (iconv_t)-1) {
        /* Get a new conversion descriptor */
        ucs2_conversion_descriptor = iconv_open("UCS-2BE", "UTF-8");
        if (ucs2_conversion_descriptor == (iconv_t)-1)
            err(EXIT_FAILURE, "Error opening the conversion context");
    } else {
        /* Reset the existing conversion descriptor */
        iconv(ucs2_conversion_descriptor, NULL, NULL, NULL, NULL);
    }

    /* Do the conversion */
    size_t rc = iconv(ucs2_conversion_descriptor, (char **)&input,
                      &input_size, (char **)&output, &output_size);
    if (rc == (size_t)-1) {
        perror("Converting to UCS-2 failed");
        free(buffer);
        if (real_strlen != NULL)
            *real_strlen = 0;
        return NULL;
    }

    /* Return the resulting string's length */
    if (real_strlen != NULL)
        *real_strlen = (buffer_size - output_size) / sizeof(xcb_char2b_t);

    return buffer;
}
