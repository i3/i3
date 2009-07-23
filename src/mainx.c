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
#include <locale.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_property.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xinerama.h>

#include <ev.h>

#include "config.h"
#include "data.h"
#include "debug.h"
#include "handlers.h"
#include "i3.h"
#include "layout.h"
#include "queue.h"
#include "table.h"
#include "util.h"
#include "xcb.h"
#include "xinerama.h"
#include "manage.h"

/* This is the path to i3, copied from argv[0] when starting up */
char **start_argv;

/* This is our connection to X11 for use with XKB */
Display *xkbdpy;

/* The list of key bindings */
struct bindings_head bindings = TAILQ_HEAD_INITIALIZER(bindings);

/* The list of exec-lines */
struct autostarts_head autostarts = TAILQ_HEAD_INITIALIZER(autostarts);

/* The list of assignments */
struct assignments_head assignments = TAILQ_HEAD_INITIALIZER(assignments);

/* This is a list of Stack_Windows, global, for easier/faster access on expose events */
struct stack_wins_head stack_wins = SLIST_HEAD_INITIALIZER(stack_wins);

/* The event handlers need to be global because they are accessed by our custom event handler
   in handle_button_press(), needed for graphical resizing */
xcb_event_handlers_t evenths;
xcb_atom_t atoms[NUM_ATOMS];

xcb_window_t root;
int num_screens = 0;

/* The depth of the root screen (used e.g. for creating new pixmaps later) */
uint8_t root_depth;

/*
 * This callback is only a dummy, see xcb_prepare_cb and xcb_check_cb.
 * See also man libev(3): "ev_prepare" and "ev_check" - customise your event loop
 *
 */
static void xcb_got_event(EV_P_ struct ev_io *w, int revents) {
        /* empty, because xcb_prepare_cb and xcb_check_cb are used */
}

/*
 * Flush before blocking (and waiting for new events)
 *
 */
static void xcb_prepare_cb(EV_P_ ev_prepare *w, int revents) {
        xcb_flush(evenths.c);
}

/*
 * Instead of polling the X connection socket we leave this to
 * xcb_poll_for_event() which knows better than we can ever know.
 *
 */
static void xcb_check_cb(EV_P_ ev_check *w, int revents) {
        xcb_generic_event_t *event;

        while ((event = xcb_poll_for_event(evenths.c)) != NULL) {
                xcb_event_handle(&evenths, event);
                free(event);
        }
}

int main(int argc, char *argv[], char *env[]) {
        int i, screens, opt;
        char *override_configpath = NULL;
        bool autostart = true;
        xcb_connection_t *conn;
        xcb_property_handlers_t prophs;
        xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];

        setlocale(LC_ALL, "");

        /* Disable output buffering to make redirects in .xsession actually useful for debugging */
        if (!isatty(fileno(stdout)))
                setbuf(stdout, NULL);

        start_argv = argv;

        while ((opt = getopt(argc, argv, "c:va")) != -1) {
                switch (opt) {
                        case 'a':
                                LOG("Autostart disabled using -a\n");
                                autostart = false;
                                break;
                        case 'c':
                                override_configpath = sstrdup(optarg);
                                break;
                        case 'v':
                                printf("i3 version " I3_VERSION " © 2009 Michael Stapelberg and contributors\n");
                                exit(EXIT_SUCCESS);
                        default:
                                fprintf(stderr, "Usage: %s [-c configfile]\n", argv[0]);
                                exit(EXIT_FAILURE);
                }
        }

        LOG("i3 version " I3_VERSION " starting\n");

        /* Initialize the table data structures for each workspace */
        init_table();

        memset(&evenths, 0, sizeof(xcb_event_handlers_t));
        memset(&prophs, 0, sizeof(xcb_property_handlers_t));

        conn = xcb_connect(NULL, &screens);

        if (xcb_connection_has_error(conn))
                die("Cannot open display\n");

        load_configuration(conn, override_configpath, false);

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
        REQUEST_ATOM(_NET_WM_WINDOW_TYPE_DIALOG);
        REQUEST_ATOM(_NET_WM_WINDOW_TYPE_UTILITY);
        REQUEST_ATOM(_NET_WM_WINDOW_TYPE_TOOLBAR);
        REQUEST_ATOM(_NET_WM_WINDOW_TYPE_SPLASH);
        REQUEST_ATOM(_NET_WM_STRUT_PARTIAL);
        REQUEST_ATOM(WM_PROTOCOLS);
        REQUEST_ATOM(WM_DELETE_WINDOW);
        REQUEST_ATOM(UTF8_STRING);
        REQUEST_ATOM(WM_STATE);

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

        /* Initialize event loop using libev */
        struct ev_loop *loop = ev_default_loop(0);
        if (loop == NULL)
                die("Could not initialize libev. Bad LIBEV_FLAGS?\n");

        struct ev_io *xcb_watcher = scalloc(sizeof(struct ev_io));
        struct ev_check *xcb_check = scalloc(sizeof(struct ev_check));
        struct ev_prepare *xcb_prepare = scalloc(sizeof(struct ev_prepare));

        ev_io_init(xcb_watcher, xcb_got_event, xcb_get_file_descriptor(conn), EV_READ);
        ev_io_start(loop, xcb_watcher);

        ev_check_init(xcb_check, xcb_check_cb);
        ev_check_start(loop, xcb_check);

        ev_prepare_init(xcb_prepare, xcb_prepare_cb);
        ev_prepare_start(loop, xcb_prepare);

        /* Grab the server to delay any events until we enter the eventloop */
        xcb_grab_server(conn);

        xcb_event_handlers_init(conn, &evenths);

        /* DEBUG: Trap all events and print them */
        for (i = 2; i < 128; ++i)
                xcb_event_set_handler(&evenths, i, handle_event, 0);

        for (i = 0; i < 256; ++i)
                xcb_event_set_error_handler(&evenths, i, (xcb_generic_error_handler_t)handle_event, 0);

        /* Expose = an Application should redraw itself, in this case it’s our titlebars. */
        xcb_event_set_expose_handler(&evenths, handle_expose_event, NULL);

        /* Key presses/releases are pretty obvious, I think */
        xcb_event_set_key_press_handler(&evenths, handle_key_press, NULL);
        xcb_event_set_key_release_handler(&evenths, handle_key_release, NULL);

        /* Enter window = user moved his mouse over the window */
        xcb_event_set_enter_notify_handler(&evenths, handle_enter_notify, NULL);

        /* Button press = user pushed a mouse button over one of our windows */
        xcb_event_set_button_press_handler(&evenths, handle_button_press, NULL);

        /* Map notify = there is a new window */
        xcb_event_set_map_request_handler(&evenths, handle_map_request, &prophs);

        /* Unmap notify = window disappeared. When sent from a client, we don’t manage
           it any longer. Usually, the client destroys the window shortly afterwards. */
        xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, NULL);

        /* Configure notify = window’s configuration (geometry, stacking, …). We only need
           it to set up ignore the following enter_notify events */
        xcb_event_set_configure_notify_handler(&evenths, handle_configure_event, NULL);

        /* Configure request = window tried to change size on its own */
        xcb_event_set_configure_request_handler(&evenths, handle_configure_request, NULL);

        /* Client message are sent to the root window. The only interesting client message
           for us is _NET_WM_STATE, we honour _NET_WM_STATE_FULLSCREEN */
        xcb_event_set_client_message_handler(&evenths, handle_client_message, NULL);

        /* Initialize the property handlers */
        xcb_property_handlers_init(&prophs, &evenths);

        /* Watch size hints (to obey correct aspect ratio) */
        xcb_property_set_handler(&prophs, WM_NORMAL_HINTS, UINT_MAX, handle_normal_hints, NULL);

        /* Get the root window and set the event mask */
        xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screens);
        root = root_screen->root;
        root_depth = root_screen->root_depth;

        uint32_t mask = XCB_CW_EVENT_MASK;
        uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
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
                        LOG("Could not get atom " #name "\n"); \
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
        GET_ATOM(_NET_WM_WINDOW_TYPE_DIALOG);
        GET_ATOM(_NET_WM_WINDOW_TYPE_UTILITY);
        GET_ATOM(_NET_WM_WINDOW_TYPE_TOOLBAR);
        GET_ATOM(_NET_WM_WINDOW_TYPE_SPLASH);
        GET_ATOM(_NET_WM_STRUT_PARTIAL);
        GET_ATOM(WM_PROTOCOLS);
        GET_ATOM(WM_DELETE_WINDOW);
        GET_ATOM(UTF8_STRING);
        GET_ATOM(WM_STATE);

        xcb_property_set_handler(&prophs, atoms[_NET_WM_WINDOW_TYPE], UINT_MAX, handle_window_type, NULL);
        /* TODO: In order to comply with EWMH, we have to watch _NET_WM_STRUT_PARTIAL */

        /* Watch _NET_WM_NAME (= title of the window in UTF-8) property */
        xcb_property_set_handler(&prophs, atoms[_NET_WM_NAME], 128, handle_windowname_change, NULL);

        /* Watch WM_TRANSIENT_FOR property (to which client this popup window belongs) */
        xcb_property_set_handler(&prophs, WM_TRANSIENT_FOR, UINT_MAX, handle_transient_for, NULL);

        /* Watch WM_NAME (= title of the window in compound text) property for legacy applications */
        xcb_watch_wm_name(&prophs, 128, handle_windowname_change_legacy, NULL);

        /* Watch WM_CLASS (= class of the window) */
        xcb_property_set_handler(&prophs, WM_CLASS, 128, handle_windowclass_change, NULL);

        /* Set up the atoms we support */
        check_error(conn, xcb_change_property_checked(conn, XCB_PROP_MODE_REPLACE, root, atoms[_NET_SUPPORTED],
                       ATOM, 32, 7, atoms), "Could not set _NET_SUPPORTED");
        /* Set up the window manager’s name */
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, atoms[_NET_SUPPORTING_WM_CHECK], WINDOW, 32, 1, &root);
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, atoms[_NET_WM_NAME], atoms[UTF8_STRING], 8, strlen("i3"), "i3");

        xcb_get_numlock_mask(conn);

        grab_all_keys(conn);

        /* Autostarting exec-lines */
        struct Autostart *exec;
        if (autostart) {
                TAILQ_FOREACH(exec, &autostarts, autostarts) {
                        LOG("auto-starting %s\n", exec->command);
                        start_application(exec->command);
                }
        }

        /* check for Xinerama */
        LOG("Checking for Xinerama...\n");
        initialize_xinerama(conn);

        xcb_flush(conn);

        manage_existing_windows(conn, &prophs, root);

        /* Get pointer position to see on which screen we’re starting */
        xcb_query_pointer_reply_t *reply;
        if ((reply = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, root), NULL)) == NULL) {
                LOG("Could not get pointer position\n");
                return 1;
        }

        i3Screen *screen = get_screen_containing(reply->root_x, reply->root_y);
        if (screen == NULL) {
                LOG("ERROR: No screen at %d x %d\n", reply->root_x, reply->root_y);
                return 0;
        }
        if (screen->current_workspace != 0) {
                LOG("Ok, I need to go to the other workspace\n");
                c_ws = &workspaces[screen->current_workspace];
        }

        /* Handle the events which arrived until now */
        xcb_check_cb(NULL, NULL, 0);

        /* Ungrab the server to receive events and enter libev’s eventloop */
        xcb_ungrab_server(conn);
        ev_loop(loop, 0);

        /* not reached */
        return 0;
}
