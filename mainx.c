#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <xcb/xcb.h>

#include "xcb_wm.h"
#include "xcb_aux.h"
#include "xcb_event.h"
#include "xcb_property.h"
#include "xcb_keysyms.h"
#include "data.h"

#include "queue.h"

Font myfont;

static const int TOP = 20;
static const int LEFT = 5;
static const int BOTTOM = 5;
static const int RIGHT = 5;

/* hm, xcb_wm wants us to implement this. */
table_t *byChild = 0;
table_t *byParent = 0;
xcb_window_t root_win;

/* We have a list of Clients, called all_clients */
LIST_HEAD(all_clients_head, Client) all_clients;

/* _the_ table. Stores all clients. */
Container *table[10][10];

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
	asprintf(&label, "gots win %p", client->window);
        xcb_void_cookie_t textCookie = xcb_image_text_8_checked (conn, strlen (label), client->window, client->titlegc, 2, 15, label );
}

void render_container(xcb_connection_t *connection, Container *container) {
	Client *client;
	int values[4];
	int mask = 	XCB_CONFIG_WINDOW_X |
			XCB_CONFIG_WINDOW_Y |
			XCB_CONFIG_WINDOW_WIDTH |
			XCB_CONFIG_WINDOW_HEIGHT;

	if (container->mode == MODE_DEFAULT) {
		LIST_FOREACH(client, &(container->clients), clients) {
			/* TODO: at the moment, every column/row is 200px. This
			 * needs to be changed to "percentage of the screen" by
			 * default and adjustable by the user if necessary.
			 */
			values[0] = container->col * 200;
			values[1] = container->row * 200;
			values[2] = 200;
			values[3] = 200;
			/* TODO: update only if necessary */
			xcb_configure_window(connection, client->window, mask, values);
		}
	} else {
		/* TODO: Implement stacking */
	}
}

void render_layout(xcb_connection_t *conn) {
	int cols, rows;

	/* Go through the whole table and render what’s necessary */
	for (rows = 0; rows < 10; rows++)
		for (cols = 0; cols < 10; cols++)
			if (table[cols][rows] != NULL) {
				/* Update position of the container */
				table[cols][rows]->row = rows;
				table[cols][rows]->col = cols;

				/* Render it */
				render_container(conn, table[cols][rows]);
			}
}

/*
 * Let’s own this window…
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
		xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
		int16_t x, int16_t y, uint16_t width, uint16_t height)
{
	Client *new = malloc(sizeof(Client));
	xcb_drawable_t drawable;
	uint32_t mask = 0;
	uint32_t values[3];
	xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

	/* Insert into the list of all clients */
	LIST_INSERT_HEAD(&all_clients, new, clients);

	/* Insert into the currently active container */
	LIST_INSERT_HEAD(&(table[current_col][current_row]->clients), new, clients);

	new->window = xcb_generate_id(conn);
	new->child = child;

	/* TODO: what do these mean? */
	mask |= XCB_CW_BACK_PIXEL;
	values[0] = root_screen->white_pixel;

	mask |= XCB_CW_OVERRIDE_REDIRECT;
	values[1] = 1;

	mask |= XCB_CW_EVENT_MASK;
	values[2] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
		| XCB_EVENT_MASK_EXPOSURE /* | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW */;

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
	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	xcb_change_window_attributes(conn, child, mask, values);

	/* TODO: At the moment, new windows just get focus */
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, new->window, XCB_CURRENT_TIME);
	
#if 0

	xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(conn, 0, strlen("_NET_ACTIVE_WINDOW"), "_NET_ACTIVE_WINDOW");
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
	int atom = -1;
	if (reply) {
		   atom = reply->atom;
		   printf("setting atom %d\n", atom);
		      free(reply);
	}
	printf("atom = %d\n", atom);


	        xcb_change_property(conn,
				XCB_PROP_MODE_REPLACE,
			root,
			atom,
			WINDOW,
			32, /* format, see http://standards.freedesktop.org/wm-spec/1.3/ar01s03.html */
			1, /* source indication? FIXME */
			&myc.window);
#endif

	render_layout(conn);

	xcb_flush(conn);
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
static int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_generic_event_t *e) {
	xcb_key_press_event_t *event = (xcb_key_press_event_t*)e;


	/* FIXME: We need to translate the keypress + state into a string (like, ä)
	   because they do not generate keysyms (use xev and see for yourself) */

	printf("oh yay!\n");
	printf("gots press %d\n", event->detail);
	printf("i'm in state %d\n", event->state);

	if (event->detail == 46) {
		/* 't' */
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
		}
	} else if (event->detail == 38) {
		//xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, myc.window, XCB_CURRENT_TIME);
	}

	//decorate_window(c, &myc);

        return format_event(e);
}

static int handle_motion(void *ignored, xcb_connection_t *conn, xcb_generic_event_t *e) {
	xcb_motion_notify_event_t *event = (xcb_motion_notify_event_t*)e;

	printf("i gots a motion: %d, %d\n", event->event_x, event->event_y);
	printf("@root that is: %d, %d\n", event->root_x, event->root_y);

	if (event->root_x < 50) {
		printf("setting focus\n");
	//xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, myc.window, XCB_CURRENT_TIME);

	}
	/* TODO: what to return? */
}


static void redrawWindow(xcb_connection_t *c, Client *client)
{
#if 0
printf("redrawing window.\n");
	xcb_drawable_t d = { client->window };
	if(!client->name_len)
		return;
	xcb_clear_area(c, 0, d, 0, 0, 0, 0);
	xcb_image_text_8(c, client->name_len, d, client->titlegc,
			LEFT - 1, TOP - 4, client->name);
	xcb_flush(c);
#endif
	decorate_window(c, client);
}

int handle_map_notify_event(void *prophs, xcb_connection_t *c, xcb_map_notify_event_t *e)
{
	window_attributes_t wa = { TAG_VALUE };
	wa.u.override_redirect = e->override_redirect;
	printf("MapNotify for 0x%08x.\n", e->window);
	manage_window(prophs, c, e->window, wa);
	return 1;
}

int handle_unmap_notify_event(void *data, xcb_connection_t *c, xcb_unmap_notify_event_t *e)
{
	Client *client = table_remove(byChild, e->event);
	xcb_window_t root;
	printf("UnmapNotify for 0x%08x (received from 0x%08x): ", e->window, e->event);
	if(!client)
	{
		printf("not a managed window. Ignoring.\n");
		return 0;
	}

	root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
	printf("child of 0x%08x.\n", client->window);
	xcb_reparent_window(c, client->child, root, 0, 0);
	xcb_destroy_window(c, client->window);
	xcb_flush(c);
	table_remove(byParent, client->window);
	free(client);
	return 1;
}



static int handleExposeEvent(void *data, xcb_connection_t *c, xcb_expose_event_t *e)
{
printf("exposeevent\n");
	Client *client = table_get(byParent, e->window);
	if(!client || e->count != 0)
		return 1;
	redrawWindow(c, client);
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
	LIST_INIT(&all_clients);
	int i, j;
	for (i = 0; i < 10; i++)
		for (j = 0; j < 10; j++)
			table[i][j] = NULL;

	/*
	 * By default, the table is one row and one column big. It contains
	 * one container in default mode in it.
	 *
	 */
	table[0][0] = calloc(sizeof(Container), 1);

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

	xcb_event_set_handler(&evenths, XCB_KEY_PRESS, handle_key_press, 0);
	xcb_event_set_handler(&evenths, XCB_MOTION_NOTIFY, handle_motion, 0);

	for(i = 0; i < 256; ++i)
		xcb_event_set_error_handler(&evenths, i, (xcb_generic_error_handler_t) handleEvent, 0);

	/* Expose = an Application should redraw itself. That is, we have to redraw our
	 * contents (= Bars) */
	xcb_event_set_expose_handler(&evenths, handleExposeEvent, 0);

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
	xcb_grab_key(c, 0, root, 0, 46, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
#if 0
   if (xcb_grab_pointer_reply(c, xcb_grab_pointer_unchecked(c, 0, root,
                                       XCB_EVENT_MASK_BUTTON_PRESS   |
                                       XCB_EVENT_MASK_BUTTON_RELEASE |
                                       XCB_EVENT_MASK_ENTER_WINDOW   |
                                       XCB_EVENT_MASK_LEAVE_WINDOW   |
                                       XCB_EVENT_MASK_POINTER_MOTION,
                                       XCB_GRAB_MODE_ASYNC,
                                       XCB_GRAB_MODE_ASYNC,
                                       XCB_NONE, XCB_NONE,
                                       XCB_CURRENT_TIME), NULL))
printf("could not grab pointer\n");
#endif



	//xcb_grab_key(c, 0, root, XCB_BUTTON_MASK_ANY, 40, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
		/* 't' */
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
