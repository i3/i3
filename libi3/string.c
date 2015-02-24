/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * string.c: Define an i3String type to automagically handle UTF-8/UCS-2
 *           conversions. Some font backends need UCS-2 (X core fonts),
 *           others want UTF-8 (Pango).
 *
 */

#include <stdlib.h>
#include <string.h>

#include "libi3.h"

struct _i3String {
    char *utf8;
    xcb_char2b_t *ucs2;
    size_t num_glyphs;
    size_t num_bytes;
};

/*
 * Build an i3String from an UTF-8 encoded string.
 * Returns the newly-allocated i3String.
 *
 */
i3String *i3string_from_utf8(const char *from_utf8) {
    i3String *str = scalloc(sizeof(i3String));

    /* Get the text */
    str->utf8 = sstrdup(from_utf8);

    /* Compute and store the length */
    str->num_bytes = strlen(str->utf8);

    return str;
}

/*
 * Build an i3String from an UTF-8 encoded string with fixed length.
 * To be used when no proper NUL-terminaison is available.
 * Returns the newly-allocated i3String.
 *
 */
i3String *i3string_from_utf8_with_length(const char *from_utf8, size_t num_bytes) {
    i3String *str = scalloc(sizeof(i3String));

    /* Copy the actual text to our i3String */
    str->utf8 = scalloc(sizeof(char) * (num_bytes + 1));
    strncpy(str->utf8, from_utf8, num_bytes);
    str->utf8[num_bytes] = '\0';

    /* Store the length */
    str->num_bytes = num_bytes;

    return str;
}

/*
 * Build an i3String from an UCS-2 encoded string.
 * Returns the newly-allocated i3String.
 *
 */
i3String *i3string_from_ucs2(const xcb_char2b_t *from_ucs2, size_t num_glyphs) {
    i3String *str = scalloc(sizeof(i3String));

    /* Copy the actual text to our i3String */
    size_t num_bytes = num_glyphs * sizeof(xcb_char2b_t);
    str->ucs2 = scalloc(num_bytes);
    memcpy(str->ucs2, from_ucs2, num_bytes);

    /* Store the length */
    str->num_glyphs = num_glyphs;

    str->utf8 = NULL;
    str->num_bytes = 0;

    return str;
}

/*
 * Free an i3String.
 *
 */
void i3string_free(i3String *str) {
    if (str == NULL)
        return;
    free(str->utf8);
    free(str->ucs2);
    free(str);
}

static void i3string_ensure_utf8(i3String *str) {
    if (str->utf8 != NULL)
        return;
    if ((str->utf8 = convert_ucs2_to_utf8(str->ucs2, str->num_glyphs)) != NULL)
        str->num_bytes = strlen(str->utf8);
}

static void i3string_ensure_ucs2(i3String *str) {
    if (str->ucs2 != NULL)
        return;
    str->ucs2 = convert_utf8_to_ucs2(str->utf8, &str->num_glyphs);
}

/*
 * Returns the UTF-8 encoded version of the i3String.
 *
 */
const char *i3string_as_utf8(i3String *str) {
    i3string_ensure_utf8(str);
    return str->utf8;
}

/*
 * Returns the UCS-2 encoded version of the i3String.
 *
 */
const xcb_char2b_t *i3string_as_ucs2(i3String *str) {
    i3string_ensure_ucs2(str);
    return str->ucs2;
}

/*
 * Returns the number of bytes (UTF-8 encoded) in an i3String.
 *
 */
size_t i3string_get_num_bytes(i3String *str) {
    i3string_ensure_utf8(str);
    return str->num_bytes;
}

/*
 * Returns the number of glyphs in an i3String.
 *
 */
size_t i3string_get_num_glyphs(i3String *str) {
    i3string_ensure_ucs2(str);
    return str->num_glyphs;
}
