/*
 * Handles font loading
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <xcb/xcb.h>

#include "data.h"

/* TODO: This is just here to be somewhere. Move it somewhere else. */
void check_error(xcb_connection_t *connection, xcb_void_cookie_t cookie, char *errMessage) {
	xcb_generic_error_t *error = xcb_request_check (connection, cookie);
	if (error != NULL) {
		fprintf(stderr, "ERROR: %s : %d\n", errMessage , error->error_code);
		xcb_disconnect(connection);
		exit(-1);
	}
}


Font *load_font(xcb_connection_t *c, const char *pattern) {
	/* TODO: this function should be caching */
	Font *new = malloc(sizeof(Font));

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
