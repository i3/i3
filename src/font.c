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

i3Font *load_font(xcb_connection_t *c, const char *pattern) {
	/* TODO: this function should be caching */
	i3Font *new = malloc(sizeof(i3Font));

	xcb_list_fonts_with_info_cookie_t cookie = xcb_list_fonts_with_info(c, 1, strlen(pattern), pattern);
	xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(c, cookie, NULL);
	if (!reply) {
		printf("Could not load font\n");
		exit(1);
	}

	/* Oh my, this is so ugly :-(. Why can’t they just return a null-terminated
	 * string? That’s what abstraction layers are for. */
	char buffer[xcb_list_fonts_with_info_name_length(reply)+1];
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, xcb_list_fonts_with_info_name(reply), sizeof(buffer)-1);
	new->name = strdup(buffer);
	new->pattern = strdup(pattern);
	new->height = reply->font_ascent + reply->font_descent;

	/* Actually load the font */
	new->id = xcb_generate_id(c);
	xcb_void_cookie_t font_cookie = xcb_open_font_checked(c, new->id, strlen(pattern), pattern);
	check_error(c, font_cookie, "Could not open font");

	return new;
}
