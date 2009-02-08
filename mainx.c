#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <xcb/xcb.h>

#include "xcb_wm.h"
#include "xcb_aux.h"
#include "xcb_event.h"
#include "xcb_property.h"
#include "xcb_keysyms.h"
#include "data.h"

#include "queue.h"
#include "table.h"

Font myfont;

static const int TOP = 20;
static const int LEFT = 5;
static const int BOTTOM = 5;
static const int RIGHT = 5;

/* hm, xcb_wm wants us to implement this. */
table_t *byChild = 0;
table_t *byParent = 0;
xcb_window_t root_win;



int current_col = 0;
int current_row = 0;


int globalc = 0;


static const char *labelError[] = {
    "Success",
    "BadRequest",
    "BadValue",
    "BadWindow",
    "BadPixmap",
    "BadAtom",
    "BadCursor",
    "BadFont",
    "BadMatch",
    "BadDrawable",
    "BadAccess",
    "BadAlloc",
    "BadColor",
    "BadGC",
    "BadIDChoice",
    "BadName",
    "BadLength",
    "BadImplementation",
};

static const char *labelRequest[] = {
    "no request",
    "CreateWindow",
    "ChangeWindowAttributes",
    "GetWindowAttributes",
    "DestroyWindow",
    "DestroySubwindows",
    "ChangeSaveSet",
    "ReparentWindow",
    "MapWindow",
    "MapSubwindows",
    "UnmapWindow",
    "UnmapSubwindows",
    "ConfigureWindow",
    "CirculateWindow",
    "GetGeometry",
    "QueryTree",
    "InternAtom",
    "GetAtomName",
    "ChangeProperty",
    "DeleteProperty",
    "GetProperty",
    "ListProperties",
    "SetSelectionOwner",
    "GetSelectionOwner",
    "ConvertSelection",
    "SendEvent",
    "GrabPointer",
    "UngrabPointer",
    "GrabButton",
    "UngrabButton",
    "ChangeActivePointerGrab",
    "GrabKeyboard",
    "UngrabKeyboard",
    "GrabKey",
    "UngrabKey",
    "AllowEvents",
    "GrabServer",
    "UngrabServer",
    "QueryPointer",
    "GetMotionEvents",
    "TranslateCoords",
    "WarpPointer",
    "SetInputFocus",
    "GetInputFocus",
    "QueryKeymap",
    "OpenFont",
    "CloseFont",
    "QueryFont",
    "QueryTextExtents",
    "ListFonts",
    "ListFontsWithInfo",
    "SetFontPath",
    "GetFontPath",
    "CreatePixmap",
    "FreePixmap",
    "CreateGC",
    "ChangeGC",
    "CopyGC",
    "SetDashes",
    "SetClipRectangles",
    "FreeGC",
    "ClearArea",
    "CopyArea",
    "CopyPlane",
    "PolyPoint",
    "PolyLine",
    "PolySegment",
    "PolyRectangle",
    "PolyArc",
    "FillPoly",
    "PolyFillRectangle",
    "PolyFillArc",
    "PutImage",
    "GetImage",
    "PolyText",
    "PolyText",
    "ImageText",
    "ImageText",
    "CreateColormap",
    "FreeColormap",
    "CopyColormapAndFree",
    "InstallColormap",
    "UninstallColormap",
    "ListInstalledColormaps",
    "AllocColor",
    "AllocNamedColor",
    "AllocColorCells",
    "AllocColorPlanes",
    "FreeColors",
    "StoreColors",
    "StoreNamedColor",
    "QueryColors",
    "LookupColor",
    "CreateCursor",
    "CreateGlyphCursor",
    "FreeCursor",
    "RecolorCursor",
    "QueryBestSize",
    "QueryExtension",
    "ListExtensions",
    "ChangeKeyboardMapping",
    "GetKeyboardMapping",
    "ChangeKeyboardControl",
    "GetKeyboardControl",
    "Bell",
    "ChangePointerControl",
    "GetPointerControl",
    "SetScreenSaver",
    "GetScreenSaver",
    "ChangeHosts",
    "ListHosts",
    "SetAccessControl",
    "SetCloseDownMode",
    "KillClient",
    "RotateProperties",
    "ForceScreenSaver",
    "SetPointerMapping",
    "GetPointerMapping",
    "SetModifierMapping",
    "GetModifierMapping",
    "major 120",
    "major 121",
    "major 122",
    "major 123",
    "major 124",
    "major 125",
    "major 126",
    "NoOperation",
};

static const char *labelEvent[] = {
    "error",
    "reply",
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify",
};

static const char *labelSendEvent[] = {
    "",
    " (from SendEvent)",
};

/*
 *
 * TODO: what exactly does this, what happens if we leave stuff out?
 *
 */
void manage_window(xcb_property_handlers_t *prophs, xcb_connection_t *c, xcb_window_t window, window_attributes_t wa)
{
	printf("managing window.\n");
	xcb_drawable_t d = { window };
	xcb_get_geometry_cookie_t geomc;
	xcb_get_geometry_reply_t *geom;
	xcb_get_window_attributes_reply_t *attr = 0;
	if(wa.tag == TAG_COOKIE)
	{
		attr = xcb_get_window_attributes_reply(c, wa.u.cookie, 0);
		if(!attr)
			return;
		if(attr->map_state != XCB_MAP_STATE_VIEWABLE)
		{
			printf("Window 0x%08x is not mapped. Ignoring.\n", window);
			free(attr);
			return;
		}
		wa.tag = TAG_VALUE;
		wa.u.override_redirect = attr->override_redirect;
	}
	if(!wa.u.override_redirect && table_get(byChild, window))
	{
		printf("Window 0x%08x already managed. Ignoring.\n", window);
		free(attr);
		return;
	}
	if(wa.u.override_redirect)
	{
		printf("Window 0x%08x has override-redirect set. Ignoring.\n", window);
		free(attr);
		return;
	}
	geomc = xcb_get_geometry(c, d);
	if(!attr)
	{
		wa.tag = TAG_COOKIE;
		wa.u.cookie = xcb_get_window_attributes(c, window);
		attr = xcb_get_window_attributes_reply(c, wa.u.cookie, 0);
	}
	geom = xcb_get_geometry_reply(c, geomc, 0);
	if(attr && geom)
	{
		reparent_window(c, window, attr->visual, geom->root, geom->depth, geom->x, geom->y, geom->width, geom->height);
		xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_NAME);
	}
	free(attr);
	free(geom);
}


/*
 * Returns the colorpixel to use for the given RGB color code
 *
 */
uint32_t get_colorpixel(xcb_connection_t *conn, xcb_window_t window, int r, int g, int b) {
	xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	xcb_colormap_t colormapId = xcb_generate_id(conn);
	xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, colormapId, window, root_screen->root_visual);
	xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(conn, xcb_alloc_color(conn, colormapId, r, g, b), NULL);

	if (!reply) {
		printf("color fail\n");
		exit(1);
	}

	uint32_t pixel = reply->pixel;
	free(reply);
	return pixel;
}

/*
 * (Re-)draws window decorations for a given Client
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client) {
	uint32_t mask = 0;
	uint32_t values[3];
	xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;

	xcb_font_t font = xcb_generate_id (conn);
	char *font_name = "-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso8859-1";
        xcb_void_cookie_t fontCookie = xcb_open_font_checked (conn, font, strlen (font_name), font_name ); 

	values[0] = root_screen->black_pixel;
	if (globalc++ > 1)
		values[1] = get_colorpixel(conn, client->window, 65535, 0, 0);
	else values[1] = get_colorpixel(conn, client->window, 0, 0, 65535);
	values[2] = font;

	xcb_change_gc(conn, client->titlegc, mask, values);


	/* TODO: utf8? */
	//char *label = "i3 rocks :>";
	char *label;
	asprintf(&label, "gots win %08x", client->window);
        xcb_void_cookie_t textCookie = xcb_image_text_8_checked (conn, strlen (label), client->window, client->titlegc, 2, 15, label );
}

void render_container(xcb_connection_t *connection, Container *container) {
	Client *client;
	uint32_t values[4];
	uint32_t mask = XCB_CONFIG_WINDOW_X |
			XCB_CONFIG_WINDOW_Y |
			XCB_CONFIG_WINDOW_WIDTH |
			XCB_CONFIG_WINDOW_HEIGHT;


	if (container->mode == MODE_DEFAULT) {
		int num_clients = 0;
		CIRCLEQ_FOREACH(client, &(container->clients), clients)
			num_clients++;
		printf("got %d clients in this default container.\n", num_clients);

		int current_client = 0;
		CIRCLEQ_FOREACH(client, &(container->clients), clients) {
			/* TODO: at the moment, every column/row is 200px. This
			 * needs to be changed to "percentage of the screen" by
			 * default and adjustable by the user if necessary.
			 */
			values[0] = container->col * container->width; /* x */
			values[1] = container->row * container->height +
				(container->height / num_clients) * current_client; /* y */
			/* TODO: vertical default layout */
			values[2] = container->width; /* width */
			values[3] = container->height / num_clients; /* height */
			printf("frame will be at %dx%d with size %dx%d\n",
					values[0], values[1], values[2], values[3]);

			/* TODO: update only if necessary */
			xcb_configure_window(connection, client->window, mask, values);

			/* The coordinates of the child are relative to its frame */
			values[0] = 2;
			values[1] = 20;
			values[2] -= 2;
			values[3] -= 20;
			printf("child itself will be at %dx%d with size %dx%d\n",
					values[0], values[1], values[2], values[3]);

			xcb_configure_window(connection, client->child, mask, values);

			decorate_window(connection, client);
			current_client++;
		}
	} else {
		/* TODO: Implement stacking */
	}
}

void render_layout(xcb_connection_t *conn) {
	int cols, rows;
	xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	int width = root_screen->width_in_pixels;
	int height = root_screen->height_in_pixels;

	int num_cols = table_dims.x, num_rows = table_dims.y;

	printf("got %d rows and %d cols\n", num_rows, num_cols);
	printf("each of them therefore is %d px width and %d px height\n",
			width / num_cols, height / num_rows);

	/* Go through the whole table and render what’s necessary */
	for (cols = 0; cols < table_dims.x; cols++)
		for (rows = 0; rows < table_dims.y; rows++)
			if (table[cols][rows] != NULL) {
				/* Update position of the container */
				table[cols][rows]->row = rows;
				table[cols][rows]->col = cols;
				table[cols][rows]->width = width / num_cols;
				table[cols][rows]->height = height / num_rows;

				/* Render it */
				render_container(conn, table[cols][rows]);
			}

	xcb_flush(conn);
}

/*
 * Let’s own this window…
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
		xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
		int16_t x, int16_t y, uint16_t width, uint16_t height)
{

	Client *new = table_get(byChild, child);
	if (new == NULL) {
		printf("oh, it's new\n");
		new = calloc(sizeof(Client), 1);
	}
	uint32_t mask = 0;
	uint32_t values[3];
	xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	/* Insert into the currently active container */
	CIRCLEQ_INSERT_TAIL(&(CUR_CELL->clients), new, clients);

	printf("currently_focused = %p\n", new);
	CUR_CELL->currently_focused = new;
	new->container = CUR_CELL;

	new->window = xcb_generate_id(conn);
	new->child = child;

	/* TODO: what do these mean? */
	mask |= XCB_CW_BACK_PIXEL;
	values[0] = root_screen->white_pixel;

	mask |= XCB_CW_OVERRIDE_REDIRECT;
	values[1] = 1;

	mask |= XCB_CW_EVENT_MASK;
	values[2] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
		| XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_ENTER_WINDOW;

	printf("Reparenting 0x%08x under 0x%08x.\n", child, new->window);

	/* Yo dawg, I heard you like windows, so I create a window around your window… */
	xcb_create_window(conn,
			depth,
			new->window,
			root,
			x,
			y,
			width + LEFT + RIGHT,
			height + TOP + BOTTOM,
			/* border_width */ 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT,
			visual,
			mask,
			values);
	xcb_change_save_set(conn, XCB_SET_MODE_INSERT, child);

	/* Map the window on the screen (= make it visible) */
	xcb_map_window(conn, new->window);

	/* Generate a graphics context for the titlebar */
	new->titlegc = xcb_generate_id(conn);
	xcb_create_gc(conn, new->titlegc, new->window, 0, 0);

	/* Draw decorations */
	decorate_window(conn, new);

	/* Put our data structure (Client) into the table */
	table_put(byParent, new->window, new);
	table_put(byChild, child, new);

	/* Moves the original window into the new frame we've created for it */
	/* TODO: hmm, LEFT/TOP needs to go */
	xcb_reparent_window(conn, child, new->window, LEFT - 1, TOP - 1);

	/* We are interested in property changes */
	mask = XCB_CW_EVENT_MASK;
	values[0] = 	XCB_EVENT_MASK_PROPERTY_CHANGE |
			XCB_EVENT_MASK_STRUCTURE_NOTIFY |
			XCB_EVENT_MASK_ENTER_WINDOW;
	xcb_change_window_attributes(conn, child, mask, values);

	/* TODO: At the moment, new windows just get focus */
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, new->window, XCB_CURRENT_TIME);

	render_layout(conn);

	xcb_flush(conn);
}

static bool focus_window_in_container(xcb_connection_t *connection, Container *container,
		direction_t direction) {
	/* If this container is empty, we’re done */
	if (container->currently_focused == NULL)
		return false;

	Client *candidad;
	if (direction == D_UP)
		candidad = CIRCLEQ_PREV(container->currently_focused, clients);
	else if (direction == D_DOWN)
		candidad = CIRCLEQ_NEXT(container->currently_focused, clients);

	/* If we don’t have anything to select, we’re done */
	if (candidad == CIRCLEQ_END(&(container->clients)))
		return false;

	/* Set focus if we could successfully move */
	container->currently_focused = candidad;
	xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE, candidad->child, XCB_CURRENT_TIME);
	xcb_flush(connection);

	return true;
}

static void focus_window(xcb_connection_t *connection, direction_t direction) {
	printf("focusing direction %d\n", direction);
	/* TODO: for horizontal default layout, this has to be expanded to LEFT/RIGHT */
	if (direction == D_UP || direction == D_DOWN) {
		/* Let’s see if we can perform up/down focus in the current container */
		Container *container = CUR_CELL;

		/* There always is a container. If not, current_col or current_row is wrong */
		assert(container != NULL);

		if (focus_window_in_container(connection, container, direction))
			return;
	} else if (direction == D_LEFT || direction == D_RIGHT) {
		if (direction == D_RIGHT && cell_exists(current_col+1, current_row))
			current_col++;
		else if (direction == D_LEFT && cell_exists(current_col-1, current_row))
			current_col--;
		else {
			printf("nah, not possible\n");
			return;
		}
		if (CUR_CELL->currently_focused != NULL) {
			xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
					CUR_CELL->currently_focused->child, XCB_CURRENT_TIME);
			xcb_flush(connection);
		}

	} else {
		printf("direction unhandled\n");
	}
}

static void move_current_window(xcb_connection_t *connection, direction_t direction) {
	printf("moving window to direction %d\n", direction);
	/* Get current window */
	Container *container = CUR_CELL;

	/* There has to be a container, see focus_window() */
	assert(container != NULL);

	/* If there is no window, we’re done */
	if (container->currently_focused == NULL)
		return;

	Client *current_client = container->currently_focused;

	if (direction == D_RIGHT) {
		printf("ok, moving right\n");
		expand_table_cols();

		Container *new = table[current_col+1][current_row];

		/* As soon as the client is moved away, the next client in the old
		 * container needs to get focus, if any. Therefore, we save it here. */
		Client *to_focus = CIRCLEQ_NEXT(current_client, clients);
		if (to_focus == CIRCLEQ_END(&(container->clients)))
			to_focus = NULL;

		/* Remove it from the old container and put it into the new one */
		CIRCLEQ_REMOVE(&(container->clients), current_client, clients);
		CIRCLEQ_INSERT_TAIL(&(new->clients), current_client, clients);

		/* Update data structures */
		current_client->container = new;
		container->currently_focused = to_focus;
		new->currently_focused = current_client;

		current_col++;

		printf("done\n");
	}

	render_layout(connection);
}

int format_event(xcb_generic_event_t *e)
{           
    uint8_t sendEvent;
    uint16_t seqnum;

    sendEvent = (e->response_type & 0x80) ? 1 : 0;
    e->response_type &= ~0x80;
    seqnum = *((uint16_t *) e + 1);

    switch(e->response_type) 
    {   
    case 0:
        printf("Error %s on seqnum %d (%s).\n",
            labelError[*((uint8_t *) e + 1)],
            seqnum,
            labelRequest[*((uint8_t *) e + 10)]);
        break;
    default:
        printf("Event %s following seqnum %d%s.\n",
            labelEvent[e->response_type],
            seqnum,
            labelSendEvent[sendEvent]);
        break;  
    case XCB_KEYMAP_NOTIFY:
        printf("Event %s%s.\n",
            labelEvent[e->response_type],
            labelSendEvent[sendEvent]);
        break;
    }

    fflush(stdout);
    return 1;
}


static int handleEvent(void *ignored, xcb_connection_t *c, xcb_generic_event_t *e)
{
        return format_event(e);
}

/*
 * There was a key press. We lookup the key symbol and see if there are any bindings
 * on that. This allows to do things like binding special characters (think of ä) to
 * functions to get one more modifier while not losing AltGr :-)
 *
 */
static int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {

	/* FIXME: We need to translate the keypress + state into a string (like, ä)
	   because they do not generate keysyms (use xev and see for yourself) */

	printf("oh yay!\n");
	printf("gots press %d\n", event->detail);
	printf("i'm in state %d\n", event->state);

	/* 30 = u
	 * 57 = n
	 * 27 = r
	 * 28 = t
	 * 40 = d
	 *
	 * …uhm, I should probably say that I’ve remapped my keys in hardware :)
	 */
	direction_t direction;
	if (event->detail == 30) {
		/* 'u' */
		pid_t pid;
		if ((pid = vfork()) == 0) {
			/* Child */
			/* TODO: what environment do we need to pass? */
			char *env[2];
			env[0] = "DISPLAY=:1";
			env[1] = NULL;
			char *argv[2];
			argv[0] = "/usr/bin/xterm";
			argv[1] = NULL;
			execve("/usr/bin/xterm", argv, env);
			/* not reached */
		}
		return 1;
	} else if (event->detail == 57) {
		direction = D_LEFT;
	} else if (event->detail == 27) {
		direction = D_DOWN;
	} else if (event->detail == 28) {
		direction = D_UP;
	} else if (event->detail == 40) {
		direction = D_RIGHT;
	} else {
		printf("don't want this.\n");
		return 1;
	}

	/* TODO: ctrl -> focus_container(conn, direction) */
	/* FIXME: actually wrong but i'm too lazy to grab my keys all the time */
	if (event->state & XCB_MOD_MASK_CONTROL) {
		move_current_window(conn, direction);
	} else

	focus_window(conn, direction);
	/* TODO: shift -> move_current_window(conn, direction) */
	/* TODO: shift + ctrl -> move_current_container(conn, direction) */

        return 1;
}

/*
 * When the user moves the mouse pointer onto a window, this callback gets called.
 *
 */
static int handle_enter_notify(void *ignored, xcb_connection_t *conn, xcb_enter_notify_event_t *event) {
	/* This was either a focus for a client’s parent (= titlebar)… */
	Client *client = table_get(byParent, event->event);
	/* …or the client itself */
	if (client == NULL)
		client = table_get(byChild, event->event);

	/* If not, then this event is not interesting. This should not happen */
	if (client == NULL) {
		printf("DEBUG: Uninteresting enter_notify-event?\n");
		return 1;
	}

	/* Update container */
	client->container->currently_focused = client;

	/* Set focus to the entered window, and flush xcb buffer immediately */
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->child, XCB_CURRENT_TIME);
	xcb_flush(conn);

	return 1;
}

int handle_map_notify_event(void *prophs, xcb_connection_t *c, xcb_map_notify_event_t *e)
{
	window_attributes_t wa = { TAG_VALUE };
	wa.u.override_redirect = e->override_redirect;
	printf("MapNotify for 0x%08x.\n", e->window);
	manage_window(prophs, c, e->window, wa);
	return 1;
}

/*
 * Our window decorations were unmapped. That means, the window will be killed now,
 * so we better clean up before.
 *
 */
int handle_unmap_notify_event(void *data, xcb_connection_t *c, xcb_unmap_notify_event_t *e) {
	Client *client = table_remove(byChild, e->event);
	xcb_window_t root;
	printf("UnmapNotify for 0x%08x (received from 0x%08x): ", e->window, e->event);
	if(!client)
	{
		printf("not a managed window. Ignoring.\n");
		return 0;
	}

	int rows, cols;
	Client *con_client;
	for (cols = 0; cols < table_dims.x; cols++)
		for (rows = 0; rows < table_dims.y; rows++)
			CIRCLEQ_FOREACH(con_client, &(table[cols][rows]->clients), clients)
				if (con_client == client) {
					printf("removing from container\n");
					CIRCLEQ_REMOVE(&(table[cols][rows]->clients), con_client, clients);
					break;
				}



	root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
	printf("child of 0x%08x.\n", client->window);
	xcb_reparent_window(c, client->child, root, 0, 0);
	xcb_destroy_window(c, client->window);
	xcb_flush(c);
	table_remove(byParent, client->window);
	free(client);

	render_layout(c);

	return 1;
}



static int handleExposeEvent(void *data, xcb_connection_t *c, xcb_expose_event_t *e)
{
printf("exposeevent\n");
	Client *client = table_get(byParent, e->window);
	if(!client || e->count != 0)
		return 1;
	decorate_window(c, client);
	return 1;
}
void manage_existing_windows(xcb_connection_t *c, xcb_property_handlers_t *prophs, xcb_window_t root)
{
	xcb_query_tree_cookie_t wintree;
	xcb_query_tree_reply_t *rep;
	int i, len;
	xcb_window_t *children;
	xcb_get_window_attributes_cookie_t *cookies;

	wintree = xcb_query_tree(c, root);
	rep = xcb_query_tree_reply(c, wintree, 0);
	if(!rep)
		return;
	len = xcb_query_tree_children_length(rep);
	cookies = malloc(len * sizeof(*cookies));
	if(!cookies)
	{
		free(rep);
		return;
	}
	children = xcb_query_tree_children(rep);
	for(i = 0; i < len; ++i)
		cookies[i] = xcb_get_window_attributes(c, children[i]);
	for(i = 0; i < len; ++i)
	{
		window_attributes_t wa = { TAG_COOKIE, { cookies[i] } };
		manage_window(prophs, c, children[i], wa);
	}
	free(rep);
}

int main() {
	int i;

	init_table();

	xcb_connection_t *c;
	xcb_event_handlers_t evenths;
	xcb_property_handlers_t prophs;
	xcb_window_t root;

	int screens;

	memset(&evenths, 0, sizeof(xcb_event_handlers_t));
	memset(&prophs, 0, sizeof(xcb_property_handlers_t));

	byChild = alloc_table();
	byParent = alloc_table();

	c = xcb_connect(NULL, &screens);

	printf("x screen is %d\n", screens);

	/* Font loading */

char *pattern = "-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso8859-1";

xcb_list_fonts_with_info_cookie_t cookie = xcb_list_fonts_with_info(c, 1, strlen(pattern), pattern);
xcb_list_fonts_with_info_reply_t *reply = xcb_list_fonts_with_info_reply(c, cookie, NULL);
if (!reply) {
	printf("Could not load font\n");
	return 1;
}

myfont.name = strdup(xcb_list_fonts_with_info_name(reply));
myfont.height = reply->font_ascent + reply->font_descent;


	xcb_event_handlers_init(c, &evenths);
	for(i = 2; i < 128; ++i)
		xcb_event_set_handler(&evenths, i, handleEvent, 0);


	for(i = 0; i < 256; ++i)
		xcb_event_set_error_handler(&evenths, i, (xcb_generic_error_handler_t) handleEvent, 0);

	/* Expose = an Application should redraw itself. That is, we have to redraw our
	 * contents (= top/bottom bar, titlebars for each window) */
	xcb_event_set_expose_handler(&evenths, handleExposeEvent, 0);

	/* Key presses are pretty obvious, I think */
	xcb_event_set_key_press_handler(&evenths, handle_key_press, 0);

	/* Enter window = user moved his mouse over the window */
	xcb_event_set_enter_notify_handler(&evenths, handle_enter_notify, 0);

	xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, 0);

	xcb_property_handlers_init(&prophs, &evenths);
	xcb_event_set_map_notify_handler(&evenths, handle_map_notify_event, &prophs);


	root = xcb_aux_get_screen(c, screens)->root;
	root_win = root;

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE };
	xcb_change_window_attributes(c, root, mask, values);

	/* Grab 'a' */
	//xcb_grab_key(c, 0, root, 0, 38, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);

	xcb_grab_key(c, 0, root, 0, 30, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(c, 0, root, XCB_BUTTON_MASK_ANY, 57, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(c, 0, root, XCB_BUTTON_MASK_ANY, 28, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(c, 0, root, XCB_BUTTON_MASK_ANY, 27, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
	xcb_grab_key(c, 0, root, XCB_BUTTON_MASK_ANY, 40, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);

	//xcb_grab_key(c, 0, root, XCB_BUTTON_MASK_ANY, 40, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
		pid_t pid;
		if ((pid = vfork()) == 0) {
			/* Child */
			/* TODO: what environment do we need to pass? */
			char *env[2];
			env[0] = "DISPLAY=:1";
			env[1] = NULL;
			char *argv[3];
			argv[0] = "/usr/bin/xterm";
			argv[1] = NULL;
			execve("/usr/bin/xterm", argv, env);
		}


	xcb_flush(c);

	manage_existing_windows(c, &prophs, root);

	xcb_event_wait_for_event_loop(&evenths);

	/* not reached */
	return 0;
}
