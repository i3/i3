/*
 * vim:ts=4:sw=4:expandtab
 */
#include <ev.h>
#include "all.h"

static int xkb_event_base;

int xkb_current_group;

extern Con *focused;

char **start_argv;

xcb_connection_t *conn;
xcb_event_handlers_t evenths;
xcb_property_handlers_t prophs;
xcb_atom_t atoms[NUM_ATOMS];

xcb_window_t root;
uint8_t root_depth;

xcb_key_symbols_t *keysyms;

/* The list of key bindings */
struct bindings_head *bindings;

/* The list of exec-lines */
struct autostarts_head autostarts = TAILQ_HEAD_INITIALIZER(autostarts);

/* The list of assignments */
struct assignments_head assignments = TAILQ_HEAD_INITIALIZER(assignments);

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
            xcb_event_handle(&evenths, event);
            free(event);
    }
}

int handle_map_request(void *unused, xcb_connection_t *conn, xcb_map_request_event_t *event) {
    xcb_get_window_attributes_cookie_t cookie;

    cookie = xcb_get_window_attributes_unchecked(conn, event->window);

    LOG("window = 0x%08x, serial is %d.\n", event->window, event->sequence);
    //add_ignore_event(event->sequence);

    manage_window(event->window, cookie, false);
    return 1;
}


int handle_unmap_notify_event(void *data, xcb_connection_t *conn, xcb_unmap_notify_event_t *event) {
    LOG("unmap event for 0x%08x\n", event->window);
    Con *con = con_by_window_id(event->window);
    if (con == NULL) {
        LOG("Not a managed window, ignoring\n");
        return 1;
    }

    tree_close(con);
    tree_render();
    return 1;
}

int handle_expose_event(void *data, xcb_connection_t *conn, xcb_expose_event_t *event) {
    Con *parent, *con;

    /* event->count is the number of minimum remaining expose events for this window, so we
       skip all events but the last one */
    if (event->count != 0)
            return 1;
    LOG("expose-event, window = %08x\n", event->window);

    if ((parent = con_by_frame_id(event->window)) == NULL) {
        LOG("expose event for unknown window, ignoring\n");
        return 1;
    }

    TAILQ_FOREACH(con, &(parent->nodes_head), nodes) {
        LOG("expose for con %p / %s\n", con, con->name);
        if (con->window)
            x_draw_decoration(con);
    }
    xcb_flush(conn);

    return 1;
}



void parse_command(const char *command) {
    printf("received command: %s\n", command);

    if (strcasecmp(command, "open") == 0)
        tree_open_con(NULL);
    else if (strcasecmp(command, "close") == 0)
        tree_close_con();
    else if (strcasecmp(command, "split h") == 0)
        tree_split(focused, HORIZ);
    else if (strcasecmp(command, "split v") == 0)
        tree_split(focused, VERT);
    else if (strcasecmp(command, "level up") == 0)
        level_up();
    else if (strcasecmp(command, "level down") == 0)
        level_down();
    else if (strcasecmp(command, "prev h") == 0)
        tree_next('p', HORIZ);
    else if (strcasecmp(command, "prev v") == 0)
        tree_next('p', VERT);
    else if (strcasecmp(command, "next h") == 0)
        tree_next('n', HORIZ);
    else if (strcasecmp(command, "next v") == 0)
        tree_next('n', VERT);
    else if (strncasecmp(command, "workspace ", strlen("workspace ")) == 0)
        workspace_show(command + strlen("workspace "));

    else if (strcasecmp(command, "move before h") == 0)
        tree_move('p', HORIZ);
    else if (strcasecmp(command, "move before v") == 0)
        tree_move('p', VERT);
    else if (strcasecmp(command, "move after h") == 0)
        tree_move('n', HORIZ);
    else if (strcasecmp(command, "move after v") == 0)
        tree_move('n', VERT);
    else if (strncasecmp(command, "restore", strlen("restore")) == 0)
        tree_append_json(command + strlen("restore "));
    else if (strncasecmp(command, "exec", strlen("exec")) == 0)
        start_application(command + strlen("exec "));
    else if (strcasecmp(command, "restart") == 0)
        i3_restart();
    else if (strcasecmp(command, "floating") == 0)
        toggle_floating_mode(focused, false);

    tree_render();

#if 0
    if (strcasecmp(command, "prev") == 0)
        tree_prev(O_CURRENT);
#endif
}

int main(int argc, char *argv[]) {
    int screens;
    char *override_configpath = NULL;
    bool autostart = true;
    bool only_check_config = false;
    bool force_xinerama = false;
    xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];
    static struct option long_options[] = {
        {"no-autostart", no_argument, 0, 'a'},
        {"config", required_argument, 0, 'c'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"force-xinerama", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    int option_index = 0, opt;

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
                printf("i3 version " I3_VERSION " © 2009 Michael Stapelberg and contributors\n");
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

    LOG("i3 (tree) version " I3_VERSION " starting\n");

    conn = xcb_connect(NULL, &screens);
    if (xcb_connection_has_error(conn))
        errx(EXIT_FAILURE, "Cannot open display\n");

    load_configuration(conn, override_configpath, false);
    if (only_check_config) {
        LOG("Done checking configuration file. Exiting.\n");
        exit(0);
    }

    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screens);
    root = root_screen->root;
    root_depth = root_screen->root_depth;

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
    REQUEST_ATOM(WM_CLIENT_LEADER);
    REQUEST_ATOM(_NET_CURRENT_DESKTOP);
    REQUEST_ATOM(_NET_ACTIVE_WINDOW);
    REQUEST_ATOM(_NET_WORKAREA);

    memset(&evenths, 0, sizeof(xcb_event_handlers_t));
    memset(&prophs, 0, sizeof(xcb_property_handlers_t));

    xcb_event_handlers_init(conn, &evenths);
    xcb_property_handlers_init(&prophs, &evenths);
    xcb_event_set_key_press_handler(&evenths, handle_key_press, NULL);

    xcb_event_set_button_press_handler(&evenths, handle_button_press, NULL);

    xcb_event_set_map_request_handler(&evenths, handle_map_request, NULL);

    xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, NULL);
    //xcb_event_set_destroy_notify_handler(&evenths, handle_destroy_notify_event, NULL);

    xcb_event_set_expose_handler(&evenths, handle_expose_event, NULL);


    /* Setup NetWM atoms */
    #define GET_ATOM(name) \
        do { \
            xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, atom_cookies[name], NULL); \
            if (!reply) { \
                ELOG("Could not get atom " #name "\n"); \
                exit(-1); \
            } \
            atoms[name] = reply->atom; \
            free(reply); \
        } while (0)

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
    GET_ATOM(WM_CLIENT_LEADER);
    GET_ATOM(_NET_CURRENT_DESKTOP);
    GET_ATOM(_NET_ACTIVE_WINDOW);
    GET_ATOM(_NET_WORKAREA);

    /* Watch _NET_WM_NAME (title of the window encoded in UTF-8) */
    xcb_property_set_handler(&prophs, atoms[_NET_WM_NAME], 128, handle_windowname_change, NULL);

    /* Watch WM_NAME (title of the window encoded in COMPOUND_TEXT) */
    xcb_watch_wm_name(&prophs, 128, handle_windowname_change_legacy, NULL);


    keysyms = xcb_key_symbols_alloc(conn);

    xcb_get_numlock_mask(conn);

    translate_keysyms();
    grab_all_keys(conn, false);

    int randr_base;
    if (force_xinerama) {
        xinerama_init();
    } else {
        DLOG("Checking for XRandR...\n");
        randr_init(&randr_base);

#if 0
        xcb_event_set_handler(&evenths,
                              randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY,
                              handle_screen_change,
                              NULL);
#endif
    }

    if (!tree_restore())
        tree_init();
    tree_render();

    /* proof-of-concept for assignments */
    Con *ws = workspace_get("3");

    Match *current_swallow = scalloc(sizeof(Match));
    TAILQ_INSERT_TAIL(&(ws->swallow_head), current_swallow, matches);

    current_swallow->insert_where = M_ACTIVE;
    current_swallow->class = strdup("xterm");

    struct ev_loop *loop = ev_loop_new(0);
    if (loop == NULL)
            die("Could not initialize libev. Bad LIBEV_FLAGS?\n");

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

    struct ev_io *xcb_watcher = scalloc(sizeof(struct ev_io));
    struct ev_check *xcb_check = scalloc(sizeof(struct ev_check));
    struct ev_prepare *xcb_prepare = scalloc(sizeof(struct ev_prepare));

    ev_io_init(xcb_watcher, xcb_got_event, xcb_get_file_descriptor(conn), EV_READ);
    ev_io_start(loop, xcb_watcher);

    ev_check_init(xcb_check, xcb_check_cb);
    ev_check_start(loop, xcb_check);

    ev_prepare_init(xcb_prepare, xcb_prepare_cb);
    ev_prepare_start(loop, xcb_prepare);

    xcb_flush(conn);

    manage_existing_windows(root);

    ev_loop(loop, 0);
}
