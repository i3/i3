/*
 * vim:ts=4:sw=4:expandtab
 */
#include <ev.h>
#include <fcntl.h>
#include "all.h"

static int xkb_event_base;

int xkb_current_group;

extern Con *focused;

char **start_argv;

xcb_connection_t *conn;

xcb_screen_t *root_screen;
xcb_window_t root;
uint8_t root_depth;

struct ev_loop *main_loop;

xcb_key_symbols_t *keysyms;

/* Those are our connections to X11 for use with libXcursor and XKB */
Display *xlibdpy, *xkbdpy;

/* The list of key bindings */
struct bindings_head *bindings;

/* The list of exec-lines */
struct autostarts_head autostarts = TAILQ_HEAD_INITIALIZER(autostarts);

/* The list of exec_always lines */
struct autostarts_always_head autostarts_always = TAILQ_HEAD_INITIALIZER(autostarts_always);

/* The list of assignments */
struct assignments_head assignments = TAILQ_HEAD_INITIALIZER(assignments);

/* The list of workspace assignments (which workspace should end up on which
 * output) */
struct ws_assignments_head ws_assignments = TAILQ_HEAD_INITIALIZER(ws_assignments);

/* We hope that those are supported and set them to true */
bool xcursor_supported = true;
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
    xcb_flush(conn);
}

/*
 * Instead of polling the X connection socket we leave this to
 * xcb_poll_for_event() which knows better than we can ever know.
 *
 */
static void xcb_check_cb(EV_P_ ev_check *w, int revents) {
    xcb_generic_event_t *event;

    while ((event = xcb_poll_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            if (event_is_ignored(event->sequence, 0))
                DLOG("Expected X11 Error received for sequence %x\n", event->sequence);
            else {
                xcb_generic_error_t *error = (xcb_generic_error_t*)event;
                ELOG("X11 Error received! sequence 0x%x, error_code = %d\n",
                     error->sequence, error->error_code);
            }
            free(event);
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
            grab_all_keys(conn, true);
        }

        if (ev.state.group == XkbGroup1Index) {
            DLOG("Mode_switch disabled\n");
            ungrab_all_keys(conn);
            grab_all_keys(conn, false);
        }
    }

    if (!mapping_changed)
        return;

    DLOG("Keyboard mapping changed, updating keybindings\n");
    xcb_key_symbols_free(keysyms);
    keysyms = xcb_key_symbols_alloc(conn);

    xcb_get_numlock_mask(conn);

    ungrab_all_keys(conn);
    DLOG("Re-grabbing...\n");
    translate_keysyms();
    grab_all_keys(conn, (xkb_current_group == XkbGroup2Index));
    DLOG("Done\n");
}

int main(int argc, char *argv[]) {
    //parse_cmd("[ foo ] attach, attach ; focus");
    int screens;
    char *override_configpath = NULL;
    bool autostart = true;
    char *layout_path = NULL;
    bool delete_layout_path = false;
    bool only_check_config = false;
    bool force_xinerama = false;
    bool disable_signalhandler = false;
    static struct option long_options[] = {
        {"no-autostart", no_argument, 0, 'a'},
        {"config", required_argument, 0, 'c'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"layout", required_argument, 0, 'L'},
        {"restart", required_argument, 0, 0},
        {"force-xinerama", no_argument, 0, 0},
        {"disable-signalhandler", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    int option_index = 0, opt;

    setlocale(LC_ALL, "");

    /* Disable output buffering to make redirects in .xsession actually useful for debugging */
    if (!isatty(fileno(stdout)))
        setbuf(stdout, NULL);

    init_logging();

    start_argv = argv;

    while ((opt = getopt_long(argc, argv, "c:CvaL:hld:V", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                LOG("Autostart disabled using -a\n");
                autostart = false;
                break;
            case 'L':
                FREE(layout_path);
                layout_path = sstrdup(optarg);
                delete_layout_path = false;
                break;
            case 'c':
                FREE(override_configpath);
                override_configpath = sstrdup(optarg);
                break;
            case 'C':
                LOG("Checking configuration file only (-C)\n");
                only_check_config = true;
                break;
            case 'v':
                printf("i3 version " I3_VERSION " © 2009-2011 Michael Stapelberg and contributors\n");
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
                } else if (strcmp(long_options[option_index].name, "disable-signalhandler") == 0) {
                    disable_signalhandler = true;
                    break;
                } else if (strcmp(long_options[option_index].name, "restart") == 0) {
                    FREE(layout_path);
                    layout_path = sstrdup(optarg);
                    delete_layout_path = true;
                    break;
                }
                /* fall-through */
            default:
                fprintf(stderr, "Usage: %s [-c configfile] [-d loglevel] [-a] [-v] [-V] [-C]\n", argv[0]);
                fprintf(stderr, "\n");
                fprintf(stderr, "-a: disable autostart\n");
                fprintf(stderr, "-L <layoutfile>: load the layout from <layoutfile>\n");
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

    LOG("i3 (tree) version " I3_VERSION " starting\n");

    conn = xcb_connect(NULL, &screens);
    if (xcb_connection_has_error(conn))
        errx(EXIT_FAILURE, "Cannot open display\n");

    /* Initialize the libev event loop. This needs to be done before loading
     * the config file because the parser will install an ev_child watcher
     * for the nagbar when config errors are found. */
    main_loop = EV_DEFAULT;
    if (main_loop == NULL)
            die("Could not initialize libev. Bad LIBEV_FLAGS?\n");

    root_screen = xcb_aux_get_screen(conn, screens);
    root = root_screen->root;
    root_depth = root_screen->root_depth;
    xcb_get_geometry_cookie_t gcookie = xcb_get_geometry(conn, root);

    load_configuration(conn, override_configpath, false);
    if (only_check_config) {
        LOG("Done checking configuration file. Exiting.\n");
        exit(0);
    }

    if (config.ipc_socket_path == NULL) {
        /* Fall back to a file name in /tmp/ based on the PID */
        if ((config.ipc_socket_path = getenv("I3SOCK")) == NULL)
            config.ipc_socket_path = get_process_filename("ipc-socket");
        else
            config.ipc_socket_path = sstrdup(config.ipc_socket_path);
    }

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

    xcb_get_geometry_reply_t *greply = xcb_get_geometry_reply(conn, gcookie, NULL);
    if (greply == NULL) {
        ELOG("Could not get geometry of the root window, exiting\n");
        return 1;
    }
    DLOG("root geometry reply: (%d, %d) %d x %d\n", greply->x, greply->y, greply->width, greply->height);

    /* Place requests for the atoms we need as soon as possible */
    #define xmacro(atom) \
        xcb_intern_atom_cookie_t atom ## _cookie = xcb_intern_atom(conn, 0, strlen(#atom), #atom);
    #include "atoms.xmacro"
    #undef xmacro

    /* Initialize the Xlib connection */
    xlibdpy = xkbdpy = XOpenDisplay(NULL);

    /* Try to load the X cursors and initialize the XKB extension */
    if (xlibdpy == NULL) {
        ELOG("ERROR: XOpenDisplay() failed, disabling libXcursor/XKB support\n");
        xcursor_supported = false;
        xkb_supported = false;
    } else if (fcntl(ConnectionNumber(xlibdpy), F_SETFD, FD_CLOEXEC) == -1) {
        ELOG("Could not set FD_CLOEXEC on xkbdpy\n");
        return 1;
    } else {
        xcursor_load_cursors();
        /*init_xkb();*/
    }

    /* Set a cursor for the root window (otherwise the root window will show no
       cursor until the first client is launched). */
    if (xcursor_supported) {
        xcursor_set_root_cursor();
    } else {
        xcb_cursor_t cursor_id = xcb_generate_id(conn);
        i3Font cursor_font = load_font("cursor", false);
        int xcb_cursor = xcursor_get_xcb_cursor(XCURSOR_CURSOR_POINTER);
        xcb_create_glyph_cursor(conn, cursor_id, cursor_font.id, cursor_font.id,
                xcb_cursor, xcb_cursor + 1, 0, 0, 0, 65535, 65535, 65535);
        xcb_change_window_attributes(conn, root, XCB_CW_CURSOR, &cursor_id);
        xcb_free_cursor(conn, cursor_id);
    }

    if (xkb_supported) {
        int errBase,
            major = XkbMajorVersion,
            minor = XkbMinorVersion;

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
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTED, A_ATOM, 32, 16, supported_atoms);
    /* Set up the window manager’s name */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTING_WM_CHECK, A_WINDOW, 32, 1, &root);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3");

    keysyms = xcb_key_symbols_alloc(conn);

    xcb_get_numlock_mask(conn);

    translate_keysyms();
    grab_all_keys(conn, false);

    bool needs_tree_init = true;
    if (layout_path) {
        LOG("Trying to restore the layout from %s...", layout_path);
        needs_tree_init = !tree_restore(layout_path, greply);
        if (delete_layout_path)
            unlink(layout_path);
        free(layout_path);
    }
    if (needs_tree_init)
        tree_init(greply);

    free(greply);

    if (force_xinerama) {
        xinerama_init();
    } else {
        DLOG("Checking for XRandR...\n");
        randr_init(&randr_base);
    }

    tree_render();

    /* Create the UNIX domain socket for IPC */
    int ipc_socket = ipc_create_socket(config.ipc_socket_path);
    if (ipc_socket == -1) {
        ELOG("Could not create the IPC socket, IPC disabled\n");
    } else {
        free(config.ipc_socket_path);
        struct ev_io *ipc_io = scalloc(sizeof(struct ev_io));
        ev_io_init(ipc_io, ipc_new_client, ipc_socket, EV_READ);
        ev_io_start(main_loop, ipc_io);
    }

    /* Set up i3 specific atoms like I3_SOCKET_PATH and I3_CONFIG_PATH */
    x_set_i3_atoms();

    struct ev_io *xcb_watcher = scalloc(sizeof(struct ev_io));
    struct ev_io *xkb = scalloc(sizeof(struct ev_io));
    struct ev_check *xcb_check = scalloc(sizeof(struct ev_check));
    struct ev_prepare *xcb_prepare = scalloc(sizeof(struct ev_prepare));

    ev_io_init(xcb_watcher, xcb_got_event, xcb_get_file_descriptor(conn), EV_READ);
    ev_io_start(main_loop, xcb_watcher);


    if (xkb_supported) {
        ev_io_init(xkb, xkb_got_event, ConnectionNumber(xkbdpy), EV_READ);
        ev_io_start(main_loop, xkb);

        /* Flush the buffer so that libev can properly get new events */
        XFlush(xkbdpy);
    }

    ev_check_init(xcb_check, xcb_check_cb);
    ev_check_start(main_loop, xcb_check);

    ev_prepare_init(xcb_prepare, xcb_prepare_cb);
    ev_prepare_start(main_loop, xcb_prepare);

    xcb_flush(conn);

    manage_existing_windows(root);

    if (!disable_signalhandler)
        setup_signal_handler();

    /* Ignore SIGPIPE to survive errors when an IPC client disconnects
     * while we are sending him a message */
    signal(SIGPIPE, SIG_IGN);

    /* Autostarting exec-lines */
    if (autostart) {
        struct Autostart *exec;
        TAILQ_FOREACH(exec, &autostarts, autostarts) {
            LOG("auto-starting %s\n", exec->command);
            start_application(exec->command);
        }
    }

    /* Autostarting exec_always-lines */
    struct Autostart *exec_always;
    TAILQ_FOREACH(exec_always, &autostarts_always, autostarts_always) {
        LOG("auto-starting (always!) %s\n", exec_always->command);
        start_application(exec_always->command);
    }

    ev_loop(main_loop, 0);
}
