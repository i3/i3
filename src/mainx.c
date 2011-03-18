/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <locale.h>
#include <fcntl.h>
#include <getopt.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>

#include <ev.h>

#include "config.h"
#include "data.h"
#include "handlers.h"
#include "click.h"
#include "i3.h"
#include "layout.h"
#include "queue.h"
#include "table.h"
#include "util.h"
#include "xcb.h"
#include "randr.h"
#include "xinerama.h"
#include "manage.h"
#include "ipc.h"
#include "log.h"
#include "sighandler.h"

static int xkb_event_base;

int xkb_current_group;

xcb_connection_t *global_conn;

/* This is the path to i3, copied from argv[0] when starting up */
char **start_argv;

/* This is our connection to X11 for use with XKB */
Display *xkbdpy;

xcb_key_symbols_t *keysyms;

/* The list of key bindings */
struct bindings_head *bindings;

/* The list of exec-lines */
struct autostarts_head autostarts = TAILQ_HEAD_INITIALIZER(autostarts);

/* The list of assignments */
struct assignments_head assignments = TAILQ_HEAD_INITIALIZER(assignments);

/* This is a list of Stack_Windows, global, for easier/faster access on expose events */
struct stack_wins_head stack_wins = SLIST_HEAD_INITIALIZER(stack_wins);

xcb_window_t root;
int num_screens = 0;

/* The depth of the root screen (used e.g. for creating new pixmaps later) */
uint8_t root_depth;

/* We hope that XKB is supported and set this to false */
bool xkb_supported = true;

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
        xcb_flush(global_conn);
}

/*
 * Instead of polling the X connection socket we leave this to
 * xcb_poll_for_event() which knows better than we can ever know.
 *
 */
static void xcb_check_cb(EV_P_ ev_check *w, int revents) {
        xcb_generic_event_t *event;

        while ((event = xcb_poll_for_event(global_conn)) != NULL) {
                if (event->response_type == 0) {
                        ELOG("X11 Error received! sequence %x\n", event->sequence);
                        continue;
                }

                /* Strip off the highest bit (set if the event is generated) */
                int type = (event->response_type & 0x7F);

                handle_event(type, event);

                free(event);
        }
}

/*
 * When using xmodmap to change the keyboard mapping, this event
 * is only sent via XKB. Therefore, we need this special handler.
 *
 */
static void xkb_got_event(EV_P_ struct ev_io *w, int revents) {
        DLOG("Handling XKB event\n");
        XkbEvent ev;

        /* When using xmodmap, every change (!) gets an own event.
         * Therefore, we just read all events and only handle the
         * mapping_notify once. */
        bool mapping_changed = false;
        while (XPending(xkbdpy)) {
                XNextEvent(xkbdpy, (XEvent*)&ev);
                /* While we should never receive a non-XKB event,
                 * better do sanity checking */
                if (ev.type != xkb_event_base)
                        continue;

                if (ev.any.xkb_type == XkbMapNotify) {
                        mapping_changed = true;
                        continue;
                }

                if (ev.any.xkb_type != XkbStateNotify) {
                        ELOG("Unknown XKB event received (type %d)\n", ev.any.xkb_type);
                        continue;
                }

                /* See The XKB Extension: Library Specification, section 14.1 */
                /* We check if the current group (each group contains
                 * two levels) has been changed. Mode_switch activates
                 * group XkbGroup2Index */
                if (xkb_current_group == ev.state.group)
                        continue;

                xkb_current_group = ev.state.group;

                if (ev.state.group == XkbGroup2Index) {
                        DLOG("Mode_switch enabled\n");
                        grab_all_keys(global_conn, true);
                }

                if (ev.state.group == XkbGroup1Index) {
                        DLOG("Mode_switch disabled\n");
                        ungrab_all_keys(global_conn);
                        grab_all_keys(global_conn, false);
                }
        }

        if (!mapping_changed)
                return;

        DLOG("Keyboard mapping changed, updating keybindings\n");
        xcb_key_symbols_free(keysyms);
        keysyms = xcb_key_symbols_alloc(global_conn);

        xcb_get_numlock_mask(global_conn);

        ungrab_all_keys(global_conn);
        DLOG("Re-grabbing...\n");
        translate_keysyms();
        grab_all_keys(global_conn, (xkb_current_group == XkbGroup2Index));
        DLOG("Done\n");
}


int main(int argc, char *argv[], char *env[]) {
        int screens, opt;
        char *override_configpath = NULL;
        bool autostart = true;
        bool only_check_config = false;
        bool force_xinerama = false;
        xcb_connection_t *conn;
        static struct option long_options[] = {
                {"no-autostart", no_argument, 0, 'a'},
                {"config", required_argument, 0, 'c'},
                {"version", no_argument, 0, 'v'},
                {"help", no_argument, 0, 'h'},
                {"force-xinerama", no_argument, 0, 0},
                {0, 0, 0, 0}
        };
        int option_index = 0;

        setlocale(LC_ALL, "");

        /* Disable output buffering to make redirects in .xsession actually useful for debugging */
        if (!isatty(fileno(stdout)))
                setbuf(stdout, NULL);

        start_argv = argv;

        while ((opt = getopt_long(argc, argv, "c:Cvahld:V", long_options, &option_index)) != -1) {
                switch (opt) {
                        case 'a':
                                LOG("Autostart disabled using -a\n");
                                autostart = false;
                                break;
                        case 'c':
                                override_configpath = sstrdup(optarg);
                                break;
                        case 'C':
                                LOG("Checking configuration file only (-C)\n");
                                only_check_config = true;
                                break;
                        case 'v':
                                printf("i3 version " I3_VERSION " © 2009-2010 Michael Stapelberg and contributors\n");
                                exit(EXIT_SUCCESS);
                        case 'V':
                                set_verbosity(true);
                                break;
                        case 'd':
                                LOG("Enabling debug loglevel %s\n", optarg);
                                add_loglevel(optarg);
                                break;
                        case 'l':
                                /* DEPRECATED, ignored for the next 3 versions (3.e, 3.f, 3.g) */
                                break;
                        case 0:
                                if (strcmp(long_options[option_index].name, "force-xinerama") == 0) {
                                        force_xinerama = true;
                                        ELOG("Using Xinerama instead of RandR. This option should be "
                                             "avoided at all cost because it does not refresh the list "
                                             "of screens, so you cannot configure displays at runtime. "
                                             "Please check if your driver really does not support RandR "
                                             "and disable this option as soon as you can.\n");
                                        break;
                                }
                                /* fall-through */
                        default:
                                fprintf(stderr, "Usage: %s [-c configfile] [-d loglevel] [-a] [-v] [-V] [-C]\n", argv[0]);
                                fprintf(stderr, "\n");
                                fprintf(stderr, "-a: disable autostart\n");
                                fprintf(stderr, "-v: display version and exit\n");
                                fprintf(stderr, "-V: enable verbose mode\n");
                                fprintf(stderr, "-d <loglevel>: enable debug loglevel <loglevel>\n");
                                fprintf(stderr, "-c <configfile>: use the provided configfile instead\n");
                                fprintf(stderr, "-C: check configuration file and exit\n");
                                fprintf(stderr, "--force-xinerama: Use Xinerama instead of RandR. This "
                                                "option should only be used if you are stuck with the "
                                                "nvidia closed source driver which does not support RandR.\n");
                                exit(EXIT_FAILURE);
                }
        }

        LOG("i3 version " I3_VERSION " starting\n");

        /* Initialize the table data structures for each workspace */
        init_table();

        conn = global_conn = xcb_connect(NULL, &screens);

        if (xcb_connection_has_error(conn))
                die("Cannot open display\n");

        /* Get the root window */
        xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screens);
        root = root_screen->root;
        root_depth = root_screen->root_depth;

        load_configuration(conn, override_configpath, false);
        if (only_check_config) {
                LOG("Done checking configuration file. Exiting.\n");
                exit(0);
        }

        /* Create the initial container on the first workspace. This used to
         * be part of init_table, but since it possibly requires an X
         * connection and a loaded configuration (default mode for new
         * containers may be stacking, which requires a new window to be
         * created), it had to be delayed. */
        expand_table_cols(TAILQ_FIRST(workspaces));
        expand_table_rows(TAILQ_FIRST(workspaces));

        /* Place requests for the atoms we need as soon as possible */
        #define xmacro(atom) \
                xcb_intern_atom_cookie_t atom ## _cookie = xcb_intern_atom(conn, 0, strlen(#atom), #atom);
        #include "atoms.xmacro"
        #undef xmacro

        /* TODO: this has to be more beautiful somewhen */
        int major, minor, error;

        major = XkbMajorVersion;
        minor = XkbMinorVersion;

        int errBase;

        if ((xkbdpy = XkbOpenDisplay(getenv("DISPLAY"), &xkb_event_base, &errBase, &major, &minor, &error)) == NULL) {
                ELOG("ERROR: XkbOpenDisplay() failed, disabling XKB support\n");
                xkb_supported = false;
        }

        if (xkb_supported) {
                if (fcntl(ConnectionNumber(xkbdpy), F_SETFD, FD_CLOEXEC) == -1) {
                        fprintf(stderr, "Could not set FD_CLOEXEC on xkbdpy\n");
                        return 1;
                }

                int i1;
                if (!XkbQueryExtension(xkbdpy,&i1,&xkb_event_base,&errBase,&major,&minor)) {
                        fprintf(stderr, "XKB not supported by X-server\n");
                        return 1;
                }
                /* end of ugliness */

                if (!XkbSelectEvents(xkbdpy, XkbUseCoreKbd,
                                     XkbMapNotifyMask | XkbStateNotifyMask,
                                     XkbMapNotifyMask | XkbStateNotifyMask)) {
                        fprintf(stderr, "Could not set XKB event mask\n");
                        return 1;
                }
        }

        /* Initialize event loop using libev */
        struct ev_loop *loop = ev_loop_new(0);
        if (loop == NULL)
                die("Could not initialize libev. Bad LIBEV_FLAGS?\n");

        struct ev_io *xcb_watcher = scalloc(sizeof(struct ev_io));
        struct ev_io *xkb = scalloc(sizeof(struct ev_io));
        struct ev_check *xcb_check = scalloc(sizeof(struct ev_check));
        struct ev_prepare *xcb_prepare = scalloc(sizeof(struct ev_prepare));

        ev_io_init(xcb_watcher, xcb_got_event, xcb_get_file_descriptor(conn), EV_READ);
        ev_io_start(loop, xcb_watcher);

        if (xkb_supported) {
                ev_io_init(xkb, xkb_got_event, ConnectionNumber(xkbdpy), EV_READ);
                ev_io_start(loop, xkb);

                /* Flush the buffer so that libev can properly get new events */
                XFlush(xkbdpy);
        }

        ev_check_init(xcb_check, xcb_check_cb);
        ev_check_start(loop, xcb_check);

        ev_prepare_init(xcb_prepare, xcb_prepare_cb);
        ev_prepare_start(loop, xcb_prepare);

        /* Grab the server to delay any events until we enter the eventloop */
        xcb_grab_server(conn);

#if 0
        /* Expose = an Application should redraw itself, in this case it’s our titlebars. */
        xcb_event_set_expose_handler(&evenths, handle_expose_event, NULL);

        /* Key presses are pretty obvious, I think */
        xcb_event_set_key_press_handler(&evenths, handle_key_press, NULL);

        /* Enter window = user moved his mouse over the window */
        xcb_event_set_enter_notify_handler(&evenths, handle_enter_notify, NULL);

        /* Button press = user pushed a mouse button over one of our windows */
        xcb_event_set_button_press_handler(&evenths, handle_button_press, NULL);

        /* Map notify = there is a new window */
        xcb_event_set_map_request_handler(&evenths, handle_map_request, &prophs);

        /* Unmap notify = window disappeared. When sent from a client, we don’t manage
           it any longer. Usually, the client destroys the window shortly afterwards. */
        xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, NULL);

        /* Destroy notify is handled the same as unmap notify */
        xcb_event_set_destroy_notify_handler(&evenths, handle_destroy_notify_event, NULL);

        /* Configure notify = window’s configuration (geometry, stacking, …). We only need
           it to set up ignore the following enter_notify events */
        xcb_event_set_configure_notify_handler(&evenths, handle_configure_event, NULL);

        /* Configure request = window tried to change size on its own */
        xcb_event_set_configure_request_handler(&evenths, handle_configure_request, NULL);

        /* Motion notify = user moved his cursor (over the root window and may
         * cross virtual screen boundaries doing that) */
        xcb_event_set_motion_notify_handler(&evenths, handle_motion_notify, NULL);

        /* Mapping notify = keyboard mapping changed (Xmodmap), re-grab bindings */
        xcb_event_set_mapping_notify_handler(&evenths, handle_mapping_notify, NULL);

        /* Client message are sent to the root window. The only interesting client message
           for us is _NET_WM_STATE, we honour _NET_WM_STATE_FULLSCREEN */
        xcb_event_set_client_message_handler(&evenths, handle_client_message, NULL);
#endif

        /* set event mask */
        uint32_t mask = XCB_CW_EVENT_MASK;
        uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                              XCB_EVENT_MASK_STRUCTURE_NOTIFY |         /* when the user adds a screen (e.g. video
                                                                           projector), the root window gets a
                                                                           ConfigureNotify */
                              XCB_EVENT_MASK_POINTER_MOTION |
                              XCB_EVENT_MASK_PROPERTY_CHANGE |
                              XCB_EVENT_MASK_ENTER_WINDOW };
        xcb_void_cookie_t cookie;
        cookie = xcb_change_window_attributes_checked(conn, root, mask, values);
        check_error(conn, cookie, "Another window manager seems to be running");

        /* Setup NetWM atoms */
        #define xmacro(name) \
                do { \
                        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, name ## _cookie, NULL); \
                        if (!reply) { \
                                ELOG("Could not get atom " #name "\n"); \
                                exit(-1); \
                        } \
                        A_ ## name = reply->atom; \
                        free(reply); \
                } while (0);
        #include "atoms.xmacro"
        #undef xmacro

        property_handlers_init();

        /* Set up the atoms we support */
        xcb_atom_t supported_atoms[] = {
#define xmacro(atom) A_ ## atom,
#include "atoms.xmacro"
#undef xmacro
        };

        /* Set up the atoms we support */
        check_error(conn, xcb_change_property_checked(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTED,
                       A_ATOM, 32, 7, supported_atoms), "Could not set _NET_SUPPORTED");
        /* Set up the window manager’s name */
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTING_WM_CHECK, A_WINDOW, 32, 1, &root);
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3");

        keysyms = xcb_key_symbols_alloc(conn);

        xcb_get_numlock_mask(conn);

        translate_keysyms();
        grab_all_keys(conn, false);

        if (force_xinerama) {
                initialize_xinerama(conn);
        } else {
                DLOG("Checking for XRandR...\n");
                initialize_randr(conn, &randr_base);
        }

        xcb_flush(conn);

        /* Get pointer position to see on which screen we’re starting */
        xcb_query_pointer_reply_t *reply;
        if ((reply = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, root), NULL)) == NULL) {
                ELOG("Could not get pointer position\n");
                return 1;
        }

        Output *screen = get_output_containing(reply->root_x, reply->root_y);
        if (screen == NULL) {
                ELOG("ERROR: No screen at %d x %d, starting on the first screen\n",
                    reply->root_x, reply->root_y);
                screen = get_first_output();
        }

        DLOG("Starting on %p\n", screen->current_workspace);
        c_ws = screen->current_workspace;

        manage_existing_windows(conn, root);

        /* Create the UNIX domain socket for IPC */
        if (config.ipc_socket_path != NULL) {
                int ipc_socket = ipc_create_socket(config.ipc_socket_path);
                if (ipc_socket == -1) {
                        ELOG("Could not create the IPC socket, IPC disabled\n");
                } else {
                        struct ev_io *ipc_io = scalloc(sizeof(struct ev_io));
                        ev_io_init(ipc_io, ipc_new_client, ipc_socket, EV_READ);
                        ev_io_start(loop, ipc_io);
                }
        }

        /* Handle the events which arrived until now */
        xcb_check_cb(NULL, NULL, 0);

        setup_signal_handler();

        /* Ignore SIGPIPE to survive errors when an IPC client disconnects
         * while we are sending him a message */
        signal(SIGPIPE, SIG_IGN);

        /* Ungrab the server to receive events and enter libev’s eventloop */
        xcb_ungrab_server(conn);

        /* Autostarting exec-lines */
        struct Autostart *exec;
        if (autostart) {
                TAILQ_FOREACH(exec, &autostarts, autostarts) {
                        LOG("auto-starting %s\n", exec->command);
                        start_application(exec->command);
                }
        }

        ev_loop(loop, 0);

        /* not reached */
        return 0;
}
