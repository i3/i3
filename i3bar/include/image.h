/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * image.c: Decoding inline images (base64-encoded PNG data URLs) for the
 * statusline
 *
 */
#pragma once

#include <config.h>

#include <stddef.h>
#include <cairo/cairo.h>

/*
 * Decodes a base64-encoded PNG data URL into a new cairo image surface.
 *
 * Returns NULL if the value is not such a data URL or if decoding the image
 * failed. On success, the caller owns the returned surface and must destroy
 * it with cairo_surface_destroy().
 *
 */
cairo_surface_t *image_surface_from_data_url(const char *value, size_t len);
