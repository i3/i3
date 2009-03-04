/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

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

#include "config.h"
#include "queue.h"
#include "table.h"
#include "layout.h"
#include "debug.h"
#include "handlers.h"
#include "util.h"
#include "xcb.h"
#include "xinerama.h"
#include "i3.h"

/* This is the path to i3, copied from argv[0] when starting up */
char *application_path;

/* This is our connection to X11 for use with XKB */
Display *xkbdpy;

/* The list of key bindings */
struct bindings_head bindings = TAILQ_HEAD_INITIALIZER(bindings);

/* This is a list of Stack_Windows, global, for easier/faster access on expose events */
struct stack_wins_head stack_wins = SLIST_HEAD_INITIALIZER(stack_wins);

/* The event handlers need to be global because they are accessed by our custom event handler
   in handle_button_press(), needed for graphical resizing */
xcb_event_handlers_t evenths;
xcb_atom_t atoms[NUM_ATOMS];

int num_screens = 0;

/*
 * Do some sanity checks and then reparent the window.
 *
 */
void manage_window(xcb_property_handlers_t *prophs, xcb_connection_t *conn, xcb_window_t window, window_attributes_t wa) {
        printf("managing window.\n");
        xcb_drawable_t d = { window };
        xcb_get_geometry_cookie_t geomc;
        xcb_get_geometry_reply_t *geom;
        xcb_get_window_attributes_reply_t *attr = 0;

        if (wa.tag == TAG_COOKIE) {
                /* Check if the window is mapped (it could be not mapped when intializing and
                   calling manage_window() for every window) */
                if ((attr = xcb_get_window_attributes_reply(conn, wa.u.cookie, 0)) == NULL)
                        return;

                if (attr->map_state != XCB_MAP_STATE_VIEWABLE)
                        goto out;

                wa.tag = TAG_VALUE;
                wa.u.override_redirect = attr->override_redirect;
        }

        /* Don’t manage clients with the override_redirect flag */
        if (wa.u.override_redirect)
                goto out;

        /* Check if the window is already managed */
        if (table_get(byChild, window))
                goto out;

        /* Get the initial geometry (position, size, …) */
        geomc = xcb_get_geometry(conn, d);
        if (!attr) {
                wa.tag = TAG_COOKIE;
                wa.u.cookie = xcb_get_window_attributes(conn, window);
                attr = xcb_get_window_attributes_reply(conn, wa.u.cookie, 0);
        }
        geom = xcb_get_geometry_reply(conn, geomc, 0);
        if (attr && geom) {
                reparent_window(conn, window, attr->visual, geom->root, geom->depth,
                                geom->x, geom->y, geom->width, geom->height);
                xcb_property_changed(prophs, XCB_PROPERTY_NEW_VALUE, window, WM_NAME);
        }

        free(geom);
out:
        free(attr);
        return;
}

/*
 * reparent_window() gets called when a new window was opened and becomes a child of the root
 * window, or it gets called by us when we manage the already existing windows at startup.
 *
 * Essentially, this is the point where we take over control.
 *
 */
void reparent_window(xcb_connection_t *conn, xcb_window_t child,
                xcb_visualid_t visual, xcb_window_t root, uint8_t depth,
                int16_t x, int16_t y, uint16_t width, uint16_t height) {

        xcb_get_property_cookie_t wm_type_cookie, strut_cookie;

        /* Place requests for properties ASAP */
        wm_type_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[_NET_WM_WINDOW_TYPE], UINT32_MAX);
        strut_cookie = xcb_get_any_property_unchecked(conn, false, child, atoms[_NET_WM_STRUT_PARTIAL], UINT32_MAX);

        Client *new = table_get(byChild, child);

        /* Events for already managed windows should already be filtered in manage_window() */
        assert(new == NULL);

        printf("reparenting new client\n");
        new = calloc(sizeof(Client), 1);
        new->force_reconfigure = true;
        uint32_t mask = 0;
        uint32_t values[3];

        /* Update the data structures */
        Client *old_focused = CUR_CELL->currently_focused;
        CUR_CELL->currently_focused = new;
        new->container = CUR_CELL;

        new->frame = xcb_generate_id(conn);
        new->child = child;
        new->rect.width = width;
        new->rect.height = height;

        /* Don’t generate events for our new window, it should *not* be managed */
        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[0] = 1;

        /* We want to know when… */
        mask |= XCB_CW_EVENT_MASK;
        values[1] =     XCB_EVENT_MASK_BUTTON_PRESS |   /* …mouse is pressed/released */
                        XCB_EVENT_MASK_BUTTON_RELEASE |
                        XCB_EVENT_MASK_EXPOSURE |       /* …our window needs to be redrawn */
                        XCB_EVENT_MASK_ENTER_WINDOW;    /* …user moves cursor inside our window */

        printf("Reparenting 0x%08x under 0x%08x.\n", child, new->frame);

        i3Font *font = load_font(conn, config.font);
        width = min(width, c_ws->rect.x + c_ws->rect.width);
        height = min(height, c_ws->rect.y + c_ws->rect.height);

        Rect framerect = {x, y,
                          width + 2 + 2,                  /* 2 px border at each side */
                          height + 2 + 2 + font->height}; /* 2 px border plus font’s height */

        /* Yo dawg, I heard you like windows, so I create a window around your window… */
        new->frame = create_window(conn, framerect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, mask, values);

        /* Put the client inside the save set. Upon termination (whether killed or normal exit
           does not matter) of the window manager, these clients will be correctly reparented
           to their most closest living ancestor (= cleanup) */
        xcb_change_save_set(conn, XCB_SET_MODE_INSERT, child);

        /* Generate a graphics context for the titlebar */
        new->titlegc = xcb_generate_id(conn);
        xcb_create_gc(conn, new->titlegc, new->frame, 0, 0);

        /* Put our data structure (Client) into the table */
        table_put(byParent, new->frame, new);
        table_put(byChild, child, new);

        /* Moves the original window into the new frame we've created for it */
        new->awaiting_useless_unmap = true;
        xcb_void_cookie_t cookie = xcb_reparent_window_checked(conn, child, new->frame, 0, font->height);
        check_error(conn, cookie, "Could not reparent window");

        /* We are interested in property changes */
        mask = XCB_CW_EVENT_MASK;
        values[0] =     XCB_EVENT_MASK_PROPERTY_CHANGE |
                        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                        XCB_EVENT_MASK_ENTER_WINDOW;
        cookie = xcb_change_window_attributes_checked(conn, child, mask, values);
        check_error(conn, cookie, "Could not change window attributes");

        /* We need to grab the mouse buttons for click to focus */
        xcb_grab_button(conn, false, child, XCB_EVENT_MASK_BUTTON_PRESS,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE,
                        1 /* left mouse button */,
                        XCB_BUTTON_MASK_ANY /* don’t filter for any modifiers */);

        /* Focus the new window */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, new->child, XCB_CURRENT_TIME);

        /* Get _NET_WM_WINDOW_TYPE (to see if it’s a dock) */
        xcb_atom_t *atom;
        xcb_get_property_reply_t *preply = xcb_get_property_reply(conn, wm_type_cookie, NULL);
        if (preply != NULL && preply->value_len > 0 && (atom = xcb_get_property_value(preply))) {
                for (int i = 0; i < xcb_get_property_value_length(preply); i++)
                        if (atom[i] == atoms[_NET_WM_WINDOW_TYPE_DOCK]) {
                                printf("Window is a dock.\n");
                                new->dock = true;
                                new->titlebar_position = TITLEBAR_OFF;
                                new->force_reconfigure = true;
                                new->container = NULL;
                                SLIST_INSERT_HEAD(&(c_ws->dock_clients), new, dock_clients);
                        }
        }

        /* Get _NET_WM_STRUT_PARTIAL to determine the client’s requested height */
        uint32_t *strut;
        preply = xcb_get_property_reply(conn, strut_cookie, NULL);
        if (preply != NULL && preply->value_len > 0 && (strut = xcb_get_property_value(preply))) {
                /* We only use a subset of the provided values, namely the reserved space at the top/bottom
                   of the screen. This is because the only possibility for bars is at to be at the top/bottom
                   with maximum horizontal size.
                   TODO: bars at the top */
                new->desired_height = strut[3];
                printf("the client wants to be %d pixels height\n", new->desired_height);
        }

        /* Insert into the currently active container, if it’s not a dock window */
        if (!new->dock) {
                /* Insert after the old active client, if existing. If it does not exist, the
                   container is empty and it does not matter, where we insert it */
                if (old_focused != NULL)
                        CIRCLEQ_INSERT_AFTER(&(CUR_CELL->clients), old_focused, new, clients);
                else CIRCLEQ_INSERT_TAIL(&(CUR_CELL->clients), new, clients);
        }

        render_layout(conn);
}

/*
 * Go through all existing windows (if the window manager is restarted) and manage them
 *
 */
void manage_existing_windows(xcb_connection_t *conn, xcb_property_handlers_t *prophs, xcb_window_t root) {
        xcb_query_tree_reply_t *reply;
        int i, len;
        xcb_window_t *children;
        xcb_get_window_attributes_cookie_t *cookies;

        /* Get the tree of windows whose parent is the root window (= all) */
        if ((reply = xcb_query_tree_reply(conn, xcb_query_tree(conn, root), 0)) == NULL)
                return;

        len = xcb_query_tree_children_length(reply);
        cookies = smalloc(len * sizeof(*cookies));

        /* Request the window attributes for every window */
        children = xcb_query_tree_children(reply);
        for(i = 0; i < len; ++i)
                cookies[i] = xcb_get_window_attributes(conn, children[i]);

        /* Call manage_window with the attributes for every window */
        for(i = 0; i < len; ++i) {
                window_attributes_t wa = { TAG_COOKIE, { cookies[i] } };
                manage_window(prophs, conn, children[i], wa);
        }

        free(reply);
}

int main(int argc, char *argv[], char *env[]) {
        int i, screens;
        xcb_connection_t *conn;
        xcb_property_handlers_t prophs;
        xcb_window_t root;
        xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];

        /* Disable output buffering to make redirects in .xsession actually useful for debugging */
        if (!isatty(fileno(stdout)))
                setbuf(stdout, NULL);

        application_path = sstrdup(argv[0]);

        /* Initialize the table data structures for each workspace */
        init_table();

        memset(&evenths, 0, sizeof(xcb_event_handlers_t));
        memset(&prophs, 0, sizeof(xcb_property_handlers_t));

        byChild = alloc_table();
        byParent = alloc_table();

        load_configuration(NULL);

        conn = xcb_connect(NULL, &screens);

        /* Place requests for the atoms we need as soon as possible */
        #define REQUEST_ATOM(name) atom_cookies[name] = xcb_intern_atom(conn, 0, strlen(#name), #name);

        REQUEST_ATOM(_NET_SUPPORTED);
        REQUEST_ATOM(_NET_WM_STATE_FULLSCREEN);
        REQUEST_ATOM(_NET_SUPPORTING_WM_CHECK);
        REQUEST_ATOM(_NET_WM_NAME);
        REQUEST_ATOM(_NET_WM_STATE);
        REQUEST_ATOM(_NET_WM_WINDOW_TYPE);
        REQUEST_ATOM(_NET_WM_DESKTOP);
        REQUEST_ATOM(_NET_WM_WINDOW_TYPE_DOCK);
        REQUEST_ATOM(_NET_WM_STRUT_PARTIAL);
        REQUEST_ATOM(UTF8_STRING);

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

        xcb_event_handlers_init(conn, &evenths);

        /* DEBUG: Trap all events and print them */
        for (i = 2; i < 128; ++i)
                xcb_event_set_handler(&evenths, i, handle_event, 0);

        for (i = 0; i < 256; ++i)
                xcb_event_set_error_handler(&evenths, i, (xcb_generic_error_handler_t)handle_event, 0);

        /* Expose = an Application should redraw itself, in this case it’s our titlebars. */
        xcb_event_set_expose_handler(&evenths, handle_expose_event, 0);

        /* Key presses/releases are pretty obvious, I think */
        xcb_event_set_key_press_handler(&evenths, handle_key_press, 0);
        xcb_event_set_key_release_handler(&evenths, handle_key_release, 0);

        /* Enter window = user moved his mouse over the window */
        xcb_event_set_enter_notify_handler(&evenths, handle_enter_notify, 0);

        /* Button press = user pushed a mouse button over one of our windows */
        xcb_event_set_button_press_handler(&evenths, handle_button_press, 0);

        /* Map notify = there is a new window */
        xcb_event_set_map_notify_handler(&evenths, handle_map_notify_event, &prophs);

        /* Unmap notify = window disappeared. When sent from a client, we don’t manage
           it any longer. Usually, the client destroys the window shortly afterwards. */
        xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, 0);

        /* Configure notify = window’s configuration (geometry, stacking, …). We only need
           it to set up ignore the following enter_notify events */
        xcb_event_set_configure_notify_handler(&evenths, handle_configure_event, 0);

        /* Client message = client changed its properties (EWMH) */
        /* TODO: can’t we do this via property handlers? */
        xcb_event_set_client_message_handler(&evenths, handle_client_message, 0);

        /* Initialize the property handlers */
        xcb_property_handlers_init(&prophs, &evenths);

        /* Watch the WM_NAME (= title of the window) property */
        xcb_watch_wm_name(&prophs, 128, handle_windowname_change, 0);

        /* Get the root window and set the event mask */
        root = xcb_aux_get_screen(conn, screens)->root;

        uint32_t mask = XCB_CW_EVENT_MASK;
        uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                              XCB_EVENT_MASK_STRUCTURE_NOTIFY |         /* when the user adds a screen (e.g. video
                                                                           projector), the root window gets a
                                                                           ConfigureNotify */
                              XCB_EVENT_MASK_PROPERTY_CHANGE |
                              XCB_EVENT_MASK_ENTER_WINDOW };
        xcb_change_window_attributes(conn, root, mask, values);

        /* Setup NetWM atoms */
        #define GET_ATOM(name) { \
                xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, atom_cookies[name], NULL); \
                if (!reply) { \
                        printf("Could not get atom " #name "\n"); \
                        exit(-1); \
                } \
                atoms[name] = reply->atom; \
                free(reply); \
        }

        GET_ATOM(_NET_SUPPORTED);
        GET_ATOM(_NET_WM_STATE_FULLSCREEN);
        GET_ATOM(_NET_SUPPORTING_WM_CHECK);
        GET_ATOM(_NET_WM_NAME);
        GET_ATOM(_NET_WM_STATE);
        GET_ATOM(_NET_WM_WINDOW_TYPE);
        GET_ATOM(_NET_WM_DESKTOP);
        GET_ATOM(_NET_WM_WINDOW_TYPE_DOCK);
        GET_ATOM(_NET_WM_STRUT_PARTIAL);
        GET_ATOM(UTF8_STRING);

        xcb_property_set_handler(&prophs, atoms[_NET_WM_WINDOW_TYPE], UINT_MAX, window_type_handler, NULL);
        /* TODO: In order to comply with EWMH, we have to watch _NET_WM_STRUT_PARTIAL */

        /* Set up the atoms we support */
        check_error(conn, xcb_change_property_checked(conn, XCB_PROP_MODE_REPLACE, root, atoms[_NET_SUPPORTED],
                       ATOM, 32, 7, atoms), "Could not set _NET_SUPPORTED");

        /* Set up the window manager’s name */
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, atoms[_NET_SUPPORTING_WM_CHECK], WINDOW, 32, 1, &root);
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, atoms[_NET_WM_NAME], atoms[UTF8_STRING], 8, strlen("i3"), "i3");

        /* Grab the bound keys */
        Binding *bind;
        TAILQ_FOREACH(bind, &bindings, bindings) {
                printf("Grabbing %d\n", bind->keycode);
                if (bind->mods & BIND_MODE_SWITCH)
                        xcb_grab_key(conn, 0, root, 0, bind->keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_SYNC);
                else xcb_grab_key(conn, 0, root, bind->mods, bind->keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC);
        }

        /* check for Xinerama */
        printf("Checking for Xinerama...\n");
        initialize_xinerama(conn);

        /* DEBUG: Start a terminal */
        start_application(config.terminal);

        xcb_flush(conn);

        manage_existing_windows(conn, &prophs, root);

        /* Get pointer position to see on which screen we’re starting */
        xcb_query_pointer_reply_t *reply;
        if ((reply = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, root), NULL)) == NULL) {
                printf("Could not get pointer position\n");
                return 1;
        }

        i3Screen *screen = get_screen_containing(reply->root_x, reply->root_y);
        if (screen == NULL) {
                printf("ERROR: No screen at %d x %d\n", reply->root_x, reply->root_y);
                return 0;
        }
        if (screen->current_workspace != 0) {
                printf("Ok, I need to go to the other workspace\n");
                c_ws = &workspaces[screen->current_workspace];
        }

        /* Enter xcb’s event handler */
        xcb_event_wait_for_event_loop(&evenths);

        /* not reached */
        return 0;
}
