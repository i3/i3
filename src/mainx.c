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

#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>

#include <xcb/xcb_wm.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_property.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xinerama.h>
#include "data.h"

#include "queue.h"
#include "table.h"
#include "font.h"
#include "layout.h"
#include "debug.h"
#include "handlers.h"
#include "util.h"

#define TERMINAL "/usr/pkg/bin/urxvt"

Display *xkbdpy;

TAILQ_HEAD(bindings_head, Binding) bindings;
xcb_event_handlers_t evenths;

static const int TOP = 20;
static const int LEFT = 5;
static const int BOTTOM = 5;
static const int RIGHT = 5;

/* hm, xcb_wm wants us to implement this. */
table_t *byChild = 0;
table_t *byParent = 0;
xcb_window_t root_win;

char *pattern = "-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso8859-1";
int num_screens = 0;

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
 * Let’s own this window…
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
		xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
		int16_t x, int16_t y, uint16_t width, uint16_t height) {

	Client *new = table_get(byChild, child);
	if (new == NULL) {
		printf("oh, it's new\n");
		new = calloc(sizeof(Client), 1);
		new->x = -1;
		new->y = -1;
	}
	uint32_t mask = 0;
	uint32_t values[3];

	/* Insert into the currently active container */
	CIRCLEQ_INSERT_TAIL(&(CUR_CELL->clients), new, clients);

	printf("currently_focused = %p\n", new);
	CUR_CELL->currently_focused = new;
	new->container = CUR_CELL;

	new->frame = xcb_generate_id(conn);
	new->child = child;
	new->width = width;
	new->height = height;

	/* Don’t generate events for our new window, it should *not* be managed */
	mask |= XCB_CW_OVERRIDE_REDIRECT;
	values[0] = 1;

	/* We want to know when… */
	mask |= XCB_CW_EVENT_MASK;
	values[1] = 	XCB_EVENT_MASK_BUTTON_PRESS | /* …mouse is pressed/released */
			XCB_EVENT_MASK_BUTTON_RELEASE |
			XCB_EVENT_MASK_EXPOSURE | /* …our window needs to be redrawn */
			XCB_EVENT_MASK_ENTER_WINDOW /* …user moves cursor inside our window */;

	printf("Reparenting 0x%08x under 0x%08x.\n", child, new->frame);

	/* Yo dawg, I heard you like windows, so I create a window around your window… */
	xcb_create_window(conn,
			depth,
			new->frame,
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
	xcb_map_window(conn, new->frame);

	/* Generate a graphics context for the titlebar */
	new->titlegc = xcb_generate_id(conn);
	xcb_create_gc(conn, new->titlegc, new->frame, 0, 0);

	/* Draw decorations */
	decorate_window(conn, new);

	/* Put our data structure (Client) into the table */
	table_put(byParent, new->frame, new);
	table_put(byChild, child, new);

	/* Moves the original window into the new frame we've created for it */
	i3Font *font = load_font(conn, pattern);
	xcb_reparent_window(conn, child, new->frame, 0, font->height);

	/* We are interested in property changes */
	mask = XCB_CW_EVENT_MASK;
	values[0] = 	XCB_EVENT_MASK_PROPERTY_CHANGE |
			XCB_EVENT_MASK_STRUCTURE_NOTIFY |
			XCB_EVENT_MASK_ENTER_WINDOW |
			XCB_EVENT_MASK_BUTTON_PRESS;
	xcb_change_window_attributes(conn, child, mask, values);

	/* We need to grab the mouse buttons for click to focus */
	xcb_grab_button(conn, false, child, XCB_EVENT_MASK_BUTTON_PRESS,
			XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1 /* left mouse button */,
			XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

	/* Focus the new window */
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, new->child, XCB_CURRENT_TIME);

	render_layout(conn);
}

void manage_existing_windows(xcb_connection_t *c, xcb_property_handlers_t *prophs, xcb_window_t root) {
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

static void initialize_xinerama(xcb_connection_t *conn) {
	xcb_xinerama_query_screens_reply_t *reply;
	xcb_xinerama_screen_info_t *screen_info;
	int screen;

	if (!xcb_get_extension_data(conn, &xcb_xinerama_id)->present) {
		printf("Xinerama extension not found, disabling.\n");
		return;
	}

	if (!xcb_xinerama_is_active_reply(conn, xcb_xinerama_is_active(conn), NULL)->state) {
		printf("Xinerama is not active (in your X-Server), disabling.\n");
		return;
	}


	reply = xcb_xinerama_query_screens_reply(conn, xcb_xinerama_query_screens_unchecked(conn), NULL);
	/* TODO: error check */
	screen_info = xcb_xinerama_query_screens_screen_info(reply);

	num_screens = xcb_xinerama_query_screens_screen_info_length(reply);

	/* Just go through each workspace and associate as many screens as we can. */
	for (screen = 0; screen < num_screens; screen++) {
		workspaces[screen].x = screen_info[screen].x_org;
		workspaces[screen].y = screen_info[screen].y_org;
		workspaces[screen].width = screen_info[screen].width;
		workspaces[screen].height = screen_info[screen].height;
		workspaces[screen].screen_num = screen;

		printf("found Xinerama screen: %d x %d at %d x %d\n",
				screen_info[screen].width, screen_info[screen].height,
				screen_info[screen].x_org, screen_info[screen].y_org);
	}

	free(screen_info);
}

int main(int argc, char *argv[], char *env[]) {
	int i, screens;
	xcb_connection_t *c;
	xcb_property_handlers_t prophs;
	xcb_window_t root;

	/* Initialize the table data structures for each workspace */
	init_table();

	memset(&evenths, 0, sizeof(xcb_event_handlers_t));
	memset(&prophs, 0, sizeof(xcb_property_handlers_t));

	byChild = alloc_table();
	byParent = alloc_table();

	TAILQ_INIT(&bindings);

	c = xcb_connect(NULL, &screens);

	/* TODO: this has to be more beautiful somewhen */
	int major, minor, error;

	major = XkbMajorVersion;
	minor = XkbMinorVersion;

	int evBase, errBase;

	if ((xkbdpy = XkbOpenDisplay(getenv("DISPLAY"), &evBase, &errBase, &major, &minor, &error)) == NULL) {
		fprintf(stderr, "XkbOpenDisplay() failed\n");
		return 1;
	}

	int i1;
	if (!XkbQueryExtension(xkbdpy,&i1,&evBase,&errBase,&major,&minor)) {
		fprintf(stderr, "XKB not supported by X-server\n");
		return 1;
	}
	/* end of ugliness */

	xcb_event_handlers_init(c, &evenths);
	for(i = 2; i < 128; ++i)
		xcb_event_set_handler(&evenths, i, handle_event, 0);

	for(i = 0; i < 256; ++i)
		xcb_event_set_error_handler(&evenths, i, (xcb_generic_error_handler_t)handle_event, 0);

	/* Expose = an Application should redraw itself. That is, we have to redraw our
	 * contents (= top/bottom bar, titlebars for each window) */
	xcb_event_set_expose_handler(&evenths, handle_expose_event, 0);

	/* Key presses/releases are pretty obvious, I think */
	xcb_event_set_key_press_handler(&evenths, handle_key_press, 0);
	xcb_event_set_key_release_handler(&evenths, handle_key_release, 0);

	/* Enter window = user moved his mouse over the window */
	xcb_event_set_enter_notify_handler(&evenths, handle_enter_notify, 0);

	/* Button press = user pushed a mouse button over one of our windows */
	xcb_event_set_button_press_handler(&evenths, handle_button_press, 0);

	xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, 0);

	xcb_property_handlers_init(&prophs, &evenths);
	xcb_event_set_map_notify_handler(&evenths, handle_map_notify_event, &prophs);

	xcb_watch_wm_name(&prophs, 128, handle_windowname_change, 0);

	root = xcb_aux_get_screen(c, screens)->root;
	root_win = root;

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE };
	xcb_change_window_attributes(c, root, mask, values);

	#define BIND(key, modifier, cmd) { \
		Binding *new = malloc(sizeof(Binding)); \
		new->keycode = key; \
		new->mods = modifier; \
		new->command = cmd; \
		TAILQ_INSERT_TAIL(&bindings, new, bindings); \
	}

	/* 38 = 'a' */
	BIND(38, BIND_MODE_SWITCH, "foo");

	BIND(30, 0, "exec /usr/pkg/bin/urxvt");

	BIND(44, BIND_MOD_1, "h");
	BIND(45, BIND_MOD_1, "j");
	BIND(46, BIND_MOD_1, "k");
	BIND(47, BIND_MOD_1, "l");

	BIND(44, BIND_MOD_1 | BIND_CONTROL, "sh");
	BIND(45, BIND_MOD_1 | BIND_CONTROL, "sj");
	BIND(46, BIND_MOD_1 | BIND_CONTROL, "sk");
	BIND(47, BIND_MOD_1 | BIND_CONTROL, "sl");

	BIND(44, BIND_MOD_1 | BIND_SHIFT, "mh");
	BIND(45, BIND_MOD_1 | BIND_SHIFT, "mj");
	BIND(46, BIND_MOD_1 | BIND_SHIFT, "mk");
	BIND(47, BIND_MOD_1 | BIND_SHIFT, "ml");

	BIND(10, BIND_MOD_1 , "1");
	BIND(11, BIND_MOD_1 , "2");
	BIND(12, BIND_MOD_1 , "3");
	BIND(13, BIND_MOD_1 , "4");
	BIND(14, BIND_MOD_1 , "5");
	BIND(15, BIND_MOD_1 , "6");
	BIND(16, BIND_MOD_1 , "7");
	BIND(17, BIND_MOD_1 , "8");
	BIND(18, BIND_MOD_1 , "9");
	BIND(19, BIND_MOD_1 , "0");

	Binding *bind;
	TAILQ_FOREACH(bind, &bindings, bindings) {
		printf("Grabbing %d\n", bind->keycode);
		if (bind->mods & BIND_MODE_SWITCH)
			xcb_grab_key(c, 0, root, 0, bind->keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_SYNC);
		else xcb_grab_key(c, 0, root, bind->mods, bind->keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
	}

	/* check for Xinerama */
	printf("Checking for Xinerama...\n");
	initialize_xinerama(c);

	start_application(TERMINAL);

	xcb_flush(c);

	manage_existing_windows(c, &prophs, root);

	xcb_event_wait_for_event_loop(&evenths);

	/* not reached */
	return 0;
}
