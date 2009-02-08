#include <xcb/xcb.h>

#ifndef _DATA_H
#define _DATA_H
/*
 * This file defines all data structures used by i3
 *
 */
#include "queue.h"

/* Forward definitions */
typedef struct Cell Cell;
typedef struct Font Font;
typedef struct Container Container;
typedef struct Client Client;

/* Helper types */
typedef enum { D_LEFT, D_RIGHT, D_UP, D_DOWN } direction_t;
struct table_dimensions_t {
	int x;
	int y;
};

/*
 * Defines a position in the table
 *
 */
struct Cell {
	int row;
	int column;
};

/*
 * We need to save the height of a font because it is required for each drawing of
 * text but relatively hard to get. As soon as a new font needs to be loaded, a
 * Font-entry will be filled for later use.
 *
 */
struct Font {
	char *name;
	int height;
};

/*
 * A client is X11-speak for a window.
 *
 */
struct Client {
	/* TODO: this is NOT final */
	Cell old_position; /* if you set a client to floating and set it back to managed,
			      it does remember its old position and *tries* to get back there */

	/* Backpointer. A client is inside a container */
	Container *container;


	/* XCB contexts */
	xcb_gcontext_t titlegc;
	xcb_window_t window;
	xcb_window_t child;

	/* The following entry provides the necessary list pointers to use Client with LIST_* macros */
	CIRCLEQ_ENTRY(Client) clients;
};

/*
 * A container is either in default or stacking mode. It sits inside the table.
 *
 */
struct Container {
	/* Those are speaking for themselves: */
	Client *currently_focused;

	/* Position of the container inside our table */
	int row;
	int col;
	/* Width/Height of the container. Changeable by the user */
	int width;
	int height;
	/* Ensure MODE_DEFAULT maps to 0 because we use calloc for initialization later */
	enum { MODE_DEFAULT = 0, MODE_STACK = 1 } mode;
	CIRCLEQ_HEAD(client_head, Client) clients;
};

#endif
