#include <xcb/xcb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcb.h"
#include "outputs.h"
#include "workspaces.h"

xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];

uint32_t get_colorpixel(const char *s) {
	char strings[3][3] = { { s[0], s[1], '\0'} ,
			       { s[2], s[3], '\0'} ,
			       { s[4], s[5], '\0'} };
	uint8_t r = strtol(strings[0], NULL, 16);
	uint8_t g = strtol(strings[1], NULL, 16);
	uint8_t b = strtol(strings[2], NULL, 16);
	return (r << 16 | g << 8 | b);
}

void init_xcb() {
	/* FIXME: xcb_connect leaks Memory */
	xcb_connection = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(xcb_connection)) {
		printf("Cannot open display\n");
		exit(EXIT_FAILURE);
	}
	printf("Connected to xcb\n");

	/* We have to request the atoms we need */
	#define ATOM_DO(name) atom_cookies[name] = xcb_intern_atom(xcb_connection, 0, strlen(#name), #name);
        #include "xcb_atoms.def"

	xcb_screens = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection)).data;
	xcb_root = xcb_screens->root;

	/* FIXME: Maybe we can push that further backwards */
	get_atoms();
}

void clean_xcb() {
	xcb_disconnect(xcb_connection);
}

void get_atoms() {
	xcb_intern_atom_reply_t* reply;
	#define ATOM_DO(name)	reply = xcb_intern_atom_reply(xcb_connection, atom_cookies[name], NULL); \
				atoms[name] = reply->atom; \
				free(reply);

	#include "xcb_atoms.def"
	printf("Got Atoms\n");
}

void destroy_windows() {
	i3_output *walk = outputs;
	while(walk != NULL) {
		if (walk->bar == XCB_NONE) {
			continue;
		}
		xcb_destroy_window(xcb_connection, walk->bar);
		walk->bar = XCB_NONE;
	}
}

void create_windows() {
	uint32_t mask;
	uint32_t values[2];

	i3_output* walk = outputs;
	while (walk != NULL) {
		if (!walk->active) {
			walk = walk->next;
			continue;
		}
		printf("Creating Window for output %s\n", walk->name);

		walk->bar = xcb_generate_id(xcb_connection);
		mask = XCB_CW_BACK_PIXEL;
		values[0] = xcb_screens->black_pixel;
		xcb_create_window(xcb_connection,
				  xcb_screens->root_depth,
				  walk->bar,
				  xcb_root,
				  walk->rect.x, walk->rect.y,
				  walk->rect.w, 20,
				  1,
				  XCB_WINDOW_CLASS_INPUT_OUTPUT,
				  xcb_screens->root_visual,
				  mask,
				  values);

		xcb_change_property(xcb_connection,
				    XCB_PROP_MODE_REPLACE,
				    walk->bar,
				    atoms[_NET_WM_WINDOW_TYPE],
				    atoms[ATOM],
				    32,
				    1,
				    (unsigned char*) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);

		walk->bargc = xcb_generate_id(xcb_connection);
		xcb_create_gc(xcb_connection,
			      walk->bargc,
			      walk->bar,
			      0,
			      NULL);

		xcb_map_window(xcb_connection, walk->bar);
		walk = walk->next;
	}
	xcb_flush(xcb_connection);
}

void draw_buttons() {
	printf("Drawing Buttons...\n");
	i3_output *outputs_walk = outputs;
	int i = 0;
	xcb_font_t button_font = xcb_generate_id(xcb_connection);
	char *fontname = "-misc-fixed-medium-r-semicondensed--12-110-75-75-c-60-iso10646-1";
	xcb_open_font(xcb_connection,
		      button_font,
		      strlen(fontname),
		      fontname);
	xcb_change_gc(xcb_connection,
		      outputs_walk->bargc,
		      XCB_GC_FONT,
		      &button_font);
	while (outputs_walk != NULL) {
		if (!outputs_walk->active) {
			printf("Output %s inactive, skipping...\n", outputs_walk->name);
			outputs_walk = outputs_walk->next;
			continue;
		}
		if (outputs_walk->bar == XCB_NONE) {
			create_windows();
		}
		uint32_t color = get_colorpixel("000000");
		xcb_change_gc(xcb_connection,
			      outputs_walk->bargc,
			      XCB_GC_FOREGROUND,
			      &color);
		xcb_rectangle_t rect = { 0, 0, outputs_walk->rect.w, 20 };
		xcb_poly_fill_rectangle(xcb_connection,
					outputs_walk->bar,
					outputs_walk->bargc,
					1,
					&rect);
		i3_ws *ws_walk = workspaces;
		while (ws_walk != NULL) {
			if (ws_walk->output != outputs_walk) {
				printf("WS %s on wrong output, skipping...\n", ws_walk->name);
				ws_walk = ws_walk->next;
				continue;
			}
			printf("Drawing Button for WS %s...\n", ws_walk->name);
			uint32_t color = get_colorpixel("240000");
			if (ws_walk->visible) {
				color = get_colorpixel("480000");
			}
			if (ws_walk->urgent) {
				color = get_colorpixel("002400");
			}
			xcb_change_gc(xcb_connection,
				      outputs_walk->bargc,
				      XCB_GC_FOREGROUND,
				      &color);
			xcb_change_gc(xcb_connection,
				      outputs_walk->bargc,
				      XCB_GC_BACKGROUND,
				      &color);
			xcb_rectangle_t rect = { i + 1, 1, 18, 18 };
			xcb_poly_fill_rectangle(xcb_connection,
						outputs_walk->bar,
						outputs_walk->bargc,
						1,
						&rect);
			color = get_colorpixel("FFFFFF");
			xcb_change_gc(xcb_connection,
				      outputs_walk->bargc,
				      XCB_GC_FOREGROUND,
				      &color);
			xcb_image_text_8(xcb_connection,
					 strlen(ws_walk->name),
					 outputs_walk->bar,
					 outputs_walk->bargc,
					 i + 3, 14,
					 ws_walk->name);
			i += 20;
			ws_walk = ws_walk->next;
		}
		outputs_walk = outputs_walk->next;
		i = 0;
	}
	xcb_flush(xcb_connection);
}
