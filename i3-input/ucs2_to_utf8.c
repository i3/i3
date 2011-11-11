/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ucs2_to_utf8.c: Converts between UCS-2 and UTF-8, both of which are used in
 *                 different contexts in X11.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <iconv.h>

#include "libi3.h"

static iconv_t conversion_descriptor = 0;
static iconv_t conversion_descriptor2 = 0;

/*
 * Returns the input string, but converted from UCS-2 to UTF-8. Memory will be
 * allocated, thus the caller has to free the output.
 *
 */
char *convert_ucs_to_utf8(char *input) {
    size_t input_size = 2;
    /* UTF-8 may consume up to 4 byte */
    int buffer_size = 8;

    char *buffer = scalloc(buffer_size);
    size_t output_size = buffer_size;
    /* We need to use an additional pointer, because iconv() modifies it */
    char *output = buffer;

    /* We convert the input into UCS-2 big endian */
    if (conversion_descriptor == 0) {
        conversion_descriptor = iconv_open("UTF-8", "UCS-2BE");
        if (conversion_descriptor == 0)
            errx(EXIT_FAILURE, "Error opening the conversion context");
    }

    /* Get the conversion descriptor back to original state */
    iconv(conversion_descriptor, NULL, NULL, NULL, NULL);

    /* Convert our text */
    int rc = iconv(conversion_descriptor, (void*)&input, &input_size, &output, &output_size);
    if (rc == (size_t)-1) {
        free(buffer);
        perror("Converting to UCS-2 failed");
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
char *convert_utf8_to_ucs2(char *input, int *real_strlen) {
    size_t input_size = strlen(input) + 1;
    /* UCS-2 consumes exactly two bytes for each glyph */
    int buffer_size = input_size * 2;

    char *buffer = smalloc(buffer_size);
    size_t output_size = buffer_size;
    /* We need to use an additional pointer, because iconv() modifies it */
    char *output = buffer;

    /* We convert the input into UCS-2 big endian */
    if (conversion_descriptor2 == 0) {
        conversion_descriptor2 = iconv_open("UCS-2BE", "UTF-8");
        if (conversion_descriptor2 == 0)
            errx(EXIT_FAILURE, "Error opening the conversion context");
    }

    /* Get the conversion descriptor back to original state */
    iconv(conversion_descriptor2, NULL, NULL, NULL, NULL);

    /* Convert our text */
    int rc = iconv(conversion_descriptor2, (void*)&input, &input_size, &output, &output_size);
    if (rc == (size_t)-1) {
        perror("Converting to UCS-2 failed");
        free(buffer);
        if (real_strlen != NULL)
            *real_strlen = 0;
        return NULL;
    }

    if (real_strlen != NULL)
        *real_strlen = ((buffer_size - output_size) / 2) - 1;

    return buffer;
}

