/*
 * This file defines all data structures used by i3
 *
 */
#include "queue.h"

/*
 * Defines a position in the table
 *
 */
typedef struct Cell {
	int row;
	int column;
} Cell;

/*
 * We need to save the height of a font because it is required for each drawing of
 * text but relatively hard to get. As soon as a new font needs to be loaded, a
 * Font-entry will be filled for later use.
 *
 */
typedef struct Font {
	char *name;
	int height;
} Font;

/*
 * A client is X11-speak for a window.
 *
 */
typedef struct Client {
	/* TODO: this is NOT final */
	Cell old_position; /* if you set a client to floating and set it back to managed,
			      it does remember its old position and *tries* to get back there */


	/* XCB contexts */
	xcb_gcontext_t titlegc;
	xcb_window_t window;
	xcb_window_t child;

	/* The following entry provides the necessary list pointers to use Client with LIST_* macros */
	LIST_ENTRY(Client) clients;
} Client;

/*
 * A container is either in default or stacking mode. It sits inside the table.
 *
 */
typedef struct Container {
	/* Position of the container inside our table */
	int row;
	int col;
	/* Width/Height of the container. Changeable by the user */
	int width;
	int height;
	/* Ensure MODE_DEFAULT maps to 0 because we use calloc for initialization later */
	enum { MODE_DEFAULT = 0, MODE_STACK = 1 } mode;
	LIST_HEAD(client_head, Client) clients;
} Container;
