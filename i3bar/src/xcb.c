#include <xcb/xcb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xcb.h"
#include "outputs.h"

xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];

void init_xcb() {
	/* LEAK: xcb_connect leaks Memory */
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

		walk->win = xcb_generate_id(xcb_connection);
		mask = XCB_CW_BACK_PIXEL;
		values[0] = xcb_screens->black_pixel;
		xcb_create_window(xcb_connection,
				  xcb_screens->root_depth,
				  walk->win,
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
				    walk->win,
				    atoms[_NET_WM_WINDOW_TYPE],
				    atoms[ATOM],
				    32,
				    1,
				    (unsigned char*) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);

		xcb_map_window(xcb_connection, walk->win);
		walk = walk->next;
	}
	xcb_flush(xcb_connection);
}

#if 0	
	xcb_screen_t* screens = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection)).data;

	xcb_gcontext_t ctx = xcb_generate_id(xcb_connection);

   	xcb_window_t win = screens->root;
   	uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
   	uint32_t values[2];
	values[0] = screens->black_pixel;
   	values[1] = 0;
	xcb_create_gc(xcb_connection, ctx, win, mask, values);

	request_atoms();

        /* Fenster erzeugen */
	win = xcb_generate_id(xcb_connection);
	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = screens->white_pixel;
	values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
	xcb_create_window(xcb_connection, screens->root_depth, win, screens->root,
		10, 10, 20, 20, 1,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, screens->root_visual,
		mask, values);

	get_atoms();

	xcb_change_property(xcb_connection,
			    XCB_PROP_MODE_REPLACE,
			    win,
			    atoms[_NET_WM_WINDOW_TYPE],
			    atoms[ATOM],
			    32,
			    1,
			    (unsigned char *) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);

	xcb_map_window(xcb_connection, win);
	 
	xcb_flush(xcb_connection);
#endif
