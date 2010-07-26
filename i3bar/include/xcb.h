#ifndef XCB_H_
#define XCB_H_

#include <xcb/xcb.h>

#define NUM_ATOMS 3

enum {
	#define ATOM_DO(name) name,
	#include "xcb_atoms.def"
};

xcb_atom_t atoms[NUM_ATOMS];

xcb_connection_t*	xcb_connection;
xcb_screen_t*		xcb_screens;
xcb_window_t		xcb_root;

void init_xcb();
void clean_xcb();
void get_atoms();
void destroy_windows();
void create_windows();
void draw_buttons();

#endif
