/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * image.c: Decoding inline images (base64-encoded PNG data URLs) for the
 * statusline
 *
 * PNG is the only supported format because cairo can decode nothing else. To
 * support more formats, image_surface_from_data_url() is the single place to
 * change: it must keep returning a cairo image surface, which is what the
 * statusline rendering works with.
 *
 */
#include "common.h"
#include "image.h"

#include <string.h>
#include <strings.h>

#include <glib.h>

/* A base64-encoded PNG data URL needs to start with this prefix. */
#define PNG_DATA_URL_PREFIX "data:image/png;base64,"

/*
 * Returns true if and only if the given string is a base64-encoded PNG data
 * URL, i.e. it starts with "data:image/png;base64,".
 *
 */
static bool is_png_data_url(const char *value, size_t len) {
    return STARTS_WITH(value, len, PNG_DATA_URL_PREFIX);
}

/* A cairo read callback source which reads from an in-memory buffer. */
typedef struct {
    const unsigned char *data;
    size_t len;
    size_t offset;
} memory_stream_t;

static cairo_status_t read_from_memory(void *closure, unsigned char *data, unsigned int length) {
    memory_stream_t *stream = closure;
    if (stream->offset + length > stream->len) {
        return CAIRO_STATUS_READ_ERROR;
    }
    memcpy(data, stream->data + stream->offset, length);
    stream->offset += length;
    return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *image_surface_from_data_url(const char *value, size_t len) {
    if (!is_png_data_url(value, len)) {
        return NULL;
    }

    const char *payload = value + strlen(PNG_DATA_URL_PREFIX);
    const size_t payload_len = len - strlen(PNG_DATA_URL_PREFIX);

    /* Decode the base64 payload. g_base64_decode_step() works on a length
     * instead of a NUL terminated string, which is what the JSON parser hands
     * us, and it ignores line breaks and other whitespace. Note that it does
     * not report failures: a malformed payload simply decodes to fewer bytes,
     * which then makes the PNG decoder below fail. */
    guchar *bin = g_new(guchar, 3 * (payload_len / 4) + 4);
    gint state = 0;
    guint save = 0;
    const gsize bin_len = g_base64_decode_step(payload, payload_len, bin, &state, &save);
    if (bin_len == 0) {
        ELOG("Could not decode base64 image data\n");
        g_free(bin);
        return NULL;
    }

    memory_stream_t stream = {
        .data = bin,
        .len = bin_len,
        .offset = 0,
    };
    cairo_surface_t *surface = cairo_image_surface_create_from_png_stream(read_from_memory, &stream);
    g_free(bin);

    const cairo_status_t status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        ELOG("Could not decode PNG image data: %s\n", cairo_status_to_string(status));
        cairo_surface_destroy(surface);
        return NULL;
    }

    DLOG("Decoded a %dx%d PNG data URL\n",
         cairo_image_surface_get_width(surface),
         cairo_image_surface_get_height(surface));

    return surface;
}
