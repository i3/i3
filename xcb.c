#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <xcb/xcb.h>
/* All the helper functions needed for efficiently using XCB */

/*
 * Returns the colorpixel to use for the given hex color (think of HTML).
 *
 * The hex_color has to start with #, for example #FF00FF.
 *
 * NOTE that get_colorpixel() does _NOT_ check the given color code for validity.
 * This has to be done by the caller.
 *
 */
uint32_t get_colorpixel(xcb_connection_t *conn, xcb_window_t window, char *hex) {
	#define RGB_8_TO_16(i) (65535 * ((i) & 0xFF) / 255)
        char strgroups[3][3] = {{hex[1], hex[2], '\0'},
                                {hex[3], hex[4], '\0'},
                                {hex[5], hex[6], '\0'}};
	int rgb16[3] = {RGB_8_TO_16(strtol(strgroups[0], NULL, 16)),
			RGB_8_TO_16(strtol(strgroups[1], NULL, 16)),
			RGB_8_TO_16(strtol(strgroups[2], NULL, 16))};

	xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	xcb_colormap_t colormapId = xcb_generate_id(conn);
	xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormapId, window, root_screen->root_visual);
	xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(conn,
			xcb_alloc_color(conn, colormapId, rgb16[0], rgb16[1], rgb16[2]), NULL);

	if (!reply) {
		printf("color fail\n");
		exit(1);
	}

	uint32_t pixel = reply->pixel;
	free(reply);
	xcb_free_colormap(conn, colormapId);
	return pixel;
}
