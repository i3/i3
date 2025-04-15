/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * string.c: Define an i3String type to automagically handle UTF-8/UCS-2
 *           conversions. Some font backends need UCS-2 (X core fonts),
 *           others want UTF-8 (Pango).
 *
 */
#include "libi3.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

struct _i3String {
    char *utf8;
    xcb_char2b_t *ucs2;
    size_t num_glyphs;
    size_t num_bytes;
    bool pango_markup;
};

/*
 * Build an i3String from an UTF-8 encoded string.
 * Returns the newly-allocated i3String.
 *
 */
i3String *i3string_from_utf8(const char *from_utf8) {
    return i3string_from_utf8_with_length(from_utf8, -1);
}

/*
 * Build an i3String from an UTF-8 encoded string in Pango markup.
 *
 */
i3String *i3string_from_markup(const char *from_markup) {
    i3String *str = i3string_from_utf8(from_markup);

    /* Set the markup flag */
    str->pango_markup = true;

    return str;
}

/*
 * Build an i3String from an UTF-8 encoded string with fixed length.
 * To be used when no proper NULL-termination is available.
 * Returns the newly-allocated i3String.
 *
 */
i3String *i3string_from_utf8_with_length(const char *from_utf8, ssize_t num_bytes) {
    i3String *str = scalloc(1, sizeof(i3String));

    /* g_utf8_make_valid NULL-terminates the string. */
    str->utf8 = g_utf8_make_valid(from_utf8, num_bytes);

    /* num_bytes < 0 means NULL-terminated string, need to calculate length */
    str->num_bytes = num_bytes < 0 ? strlen(str->utf8) : (size_t)num_bytes;

    return str;
}

/*
 * Build an i3String from an UTF-8 encoded string in Pango markup with fixed
 * length.
 *
 */
i3String *i3string_from_markup_with_length(const char *from_markup, size_t num_bytes) {
    i3String *str = i3string_from_utf8_with_length(from_markup, num_bytes);

    /* set the markup flag */
    str->pango_markup = true;

    return str;
}

/*
 * Build an i3String from an UCS-2 encoded string.
 * Returns the newly-allocated i3String.
 *
 */
i3String *i3string_from_ucs2(const xcb_char2b_t *from_ucs2, size_t num_glyphs) {
    i3String *str = scalloc(1, sizeof(i3String));

    /* Copy the actual text to our i3String */
    str->ucs2 = scalloc(num_glyphs, sizeof(xcb_char2b_t));
    memcpy(str->ucs2, from_ucs2, num_glyphs * sizeof(xcb_char2b_t));

    /* Store the length */
    str->num_glyphs = num_glyphs;

    str->utf8 = NULL;
    str->num_bytes = 0;

    return str;
}

/*
 * Copies the given i3string.
 * Note that this will not free the source string.
 */
i3String *i3string_copy(i3String *str) {
    i3String *copy = i3string_from_utf8(i3string_as_utf8(str));
    copy->pango_markup = str->pango_markup;
    return copy;
}

/*
 * Free an i3String.
 *
 */
void i3string_free(i3String *str) {
    if (str == NULL) {
        return;
    }
    free(str->utf8);
    free(str->ucs2);
    free(str);
}

static void i3string_ensure_utf8(i3String *str) {
    if (str->utf8 != NULL) {
        return;
    }
    if ((str->utf8 = convert_ucs2_to_utf8(str->ucs2, str->num_glyphs)) != NULL) {
        str->num_bytes = strlen(str->utf8);
    }
}

static void i3string_ensure_ucs2(i3String *str) {
    if (str->ucs2 != NULL) {
        return;
    }
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
 * Whether the given i3String is in Pango markup.
 */
bool i3string_is_markup(i3String *str) {
    return str->pango_markup;
}

/*
 * Set whether the i3String should use Pango markup.
 */
void i3string_set_markup(i3String *str, bool pango_markup) {
    str->pango_markup = pango_markup;
}

/*
 * Escape pango markup characters in the given string.
 */
i3String *i3string_escape_markup(i3String *str) {
    const char *text = i3string_as_utf8(str);
    char *escaped = g_markup_escape_text(text, -1);
    i3String *result = i3string_from_utf8(escaped);
    free(escaped);
    return result;
}

/*
 * Returns the number of glyphs in an i3String.
 *
 */
size_t i3string_get_num_glyphs(i3String *str) {
    i3string_ensure_ucs2(str);
    return str->num_glyphs;
}
