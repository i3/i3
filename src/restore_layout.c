#undef I3__FILE__
#define I3__FILE__ "restore_layout.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2013 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * restore_layout.c: Everything for restored containers that is not pure state
 *                   parsing (which can be found in load_layout.c).
 *
 *
 */
#include "all.h"

typedef struct placeholder_state {
    /** The X11 placeholder window. */
    xcb_window_t window;
    /** The container to which this placeholder window belongs. */
    Con *con;

    /** Current size of the placeholder window (to detect size changes). */
    Rect rect;

    /** The pixmap to render on (back buffer). */
    xcb_pixmap_t pixmap;
    /** The graphics context for “pixmap”. */
    xcb_gcontext_t gc;

    TAILQ_ENTRY(placeholder_state) state;
} placeholder_state;

static TAILQ_HEAD(state_head, placeholder_state) state_head =
    TAILQ_HEAD_INITIALIZER(state_head);

static xcb_connection_t *restore_conn;

static struct ev_io *xcb_watcher;
static struct ev_check *xcb_check;
static struct ev_prepare *xcb_prepare;

static void restore_handle_event(int type, xcb_generic_event_t *event);

/* Documentation for these functions can be found in src/main.c, starting at xcb_got_event */
static void restore_xcb_got_event(EV_P_ struct ev_io *w, int revents) {
}

static void restore_xcb_prepare_cb(EV_P_ ev_prepare *w, int revents) {
    xcb_flush(restore_conn);
}

static void restore_xcb_check_cb(EV_P_ ev_check *w, int revents) {
    xcb_generic_event_t *event;

    if (xcb_connection_has_error(restore_conn)) {
        DLOG("restore X11 connection has an error, reconnecting\n");
        restore_connect();
        return;
    }

    while ((event = xcb_poll_for_event(restore_conn)) != NULL) {
        if (event->response_type == 0) {
            xcb_generic_error_t *error = (xcb_generic_error_t*)event;
            DLOG("X11 Error received (probably harmless)! sequence 0x%x, error_code = %d\n",
                 error->sequence, error->error_code);
            free(event);
            continue;
        }

        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);

        restore_handle_event(type, event);

        free(event);
    }
}

/*
 * Opens a separate connection to X11 for placeholder windows when restoring
 * layouts. This is done as a safety measure (users can xkill a placeholder
 * window without killing their window manager) and for better isolation, both
 * on the wire to X11 and thus also in the code.
 *
 */
void restore_connect(void) {
    if (restore_conn != NULL) {
        /* This is not the initial connect, but a reconnect, most likely
         * because our X11 connection was killed (e.g. by a user with xkill. */
        ev_io_stop(main_loop, xcb_watcher);
        ev_check_stop(main_loop, xcb_check);
        ev_prepare_stop(main_loop, xcb_prepare);

        placeholder_state *state;
        while (!TAILQ_EMPTY(&state_head)) {
            state = TAILQ_FIRST(&state_head);
            TAILQ_REMOVE(&state_head, state, state);
            free(state);
        }

        free(restore_conn);
        free(xcb_watcher);
        free(xcb_check);
        free(xcb_prepare);
    }

    int screen;
    restore_conn = xcb_connect(NULL, &screen);
    if (restore_conn == NULL || xcb_connection_has_error(restore_conn))
        errx(EXIT_FAILURE, "Cannot open display\n");

    xcb_watcher = scalloc(sizeof(struct ev_io));
    xcb_check = scalloc(sizeof(struct ev_check));
    xcb_prepare = scalloc(sizeof(struct ev_prepare));

    ev_io_init(xcb_watcher, restore_xcb_got_event, xcb_get_file_descriptor(restore_conn), EV_READ);
    ev_io_start(main_loop, xcb_watcher);

    ev_check_init(xcb_check, restore_xcb_check_cb);
    ev_check_start(main_loop, xcb_check);

    ev_prepare_init(xcb_prepare, restore_xcb_prepare_cb);
    ev_prepare_start(main_loop, xcb_prepare);
}

static void update_placeholder_contents(placeholder_state *state) {
    xcb_change_gc(restore_conn, state->gc, XCB_GC_FOREGROUND,
                  (uint32_t[]) { config.client.placeholder.background });
    xcb_poly_fill_rectangle(restore_conn, state->pixmap, state->gc, 1,
            (xcb_rectangle_t[]) { { 0, 0, state->rect.width, state->rect.height } });

    // TODO: make i3font functions per-connection, at least these two for now…?
    xcb_flush(restore_conn);
    xcb_aux_sync(restore_conn);

    set_font_colors(state->gc, config.client.placeholder.text, config.client.placeholder.background);

    Match *swallows;
    int n = 0;
    TAILQ_FOREACH(swallows, &(state->con->swallow_head), matches) {
        char *serialized = NULL;

#define APPEND_REGEX(re_name) do { \
    if (swallows->re_name != NULL) { \
        sasprintf(&serialized, "%s%s" #re_name "=\"%s\"", \
                  (serialized ? serialized : "["), \
                  (serialized ? " " : ""), \
                  swallows->re_name->pattern); \
    } \
} while (0)

        APPEND_REGEX(class);
        APPEND_REGEX(instance);
        APPEND_REGEX(window_role);
        APPEND_REGEX(title);

        if (serialized == NULL) {
            DLOG("This swallows specification is not serializable?!\n");
            continue;
        }

        sasprintf(&serialized, "%s]", serialized);
        DLOG("con %p (placeholder 0x%08x) line %d: %s\n", state->con, state->window, n, serialized);

        i3String *str = i3string_from_utf8(serialized);
        draw_text(str, state->pixmap, state->gc, 2, (n * (config.font.height + 2)) + 2, state->rect.width - 2);
        i3string_free(str);
        n++;
        free(serialized);
    }

    // TODO: render the watch symbol in a bigger font
    i3String *line = i3string_from_utf8("⌚");
    int text_width = predict_text_width(line);
    int x = (state->rect.width / 2) - (text_width / 2);
    int y = (state->rect.height / 2) - (config.font.height / 2);
    draw_text(line, state->pixmap, state->gc, x, y, text_width);
    i3string_free(line);
    xcb_flush(conn);
    xcb_aux_sync(conn);
}

static void open_placeholder_window(Con *con) {
    if (con_is_leaf(con) &&
        (con->window == NULL || con->window->id == XCB_NONE)) {
        xcb_window_t placeholder = create_window(
                restore_conn,
                con->rect,
                XCB_COPY_FROM_PARENT,
                XCB_COPY_FROM_PARENT,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                XCURSOR_CURSOR_POINTER,
                true,
                XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                (uint32_t[]){
                    config.client.placeholder.background,
                    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                });
        /* Set the same name as was stored in the layout file. While perhaps
         * slightly confusing in the first instant, this brings additional
         * clarity to which placeholder is waiting for which actual window. */
        xcb_change_property(restore_conn, XCB_PROP_MODE_REPLACE, placeholder,
                            A__NET_WM_NAME, A_UTF8_STRING, 8, strlen(con->name), con->name);
        DLOG("Created placeholder window 0x%08x for leaf container %p / %s\n",
             placeholder, con, con->name);

        placeholder_state *state = scalloc(sizeof(placeholder_state));
        state->window = placeholder;
        state->con = con;
        state->rect = con->rect;
        state->pixmap = xcb_generate_id(restore_conn);
        xcb_create_pixmap(restore_conn, root_depth, state->pixmap,
                          state->window, state->rect.width, state->rect.height);
        state->gc = xcb_generate_id(restore_conn);
        xcb_create_gc(restore_conn, state->gc, state->pixmap, XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){ 0 });
        update_placeholder_contents(state);
        TAILQ_INSERT_TAIL(&state_head, state, state);

        /* create temporary id swallow to match the placeholder */
        Match *temp_id = smalloc(sizeof(Match));
        match_init(temp_id);
        temp_id->id = placeholder;
        TAILQ_INSERT_TAIL(&(con->swallow_head), temp_id, matches);
    }

    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        open_placeholder_window(child);
    }
    TAILQ_FOREACH(child, &(con->floating_head), floating_windows) {
        open_placeholder_window(child);
    }
}

/*
 * Open placeholder windows for all children of parent. The placeholder window
 * will vanish as soon as a real window is swallowed by the container. Until
 * then, it exposes the criteria that must be fulfilled for a window to be
 * swallowed by this container.
 *
 */
void restore_open_placeholder_windows(Con *parent) {
    Con *child;
    TAILQ_FOREACH(child, &(parent->nodes_head), nodes) {
        open_placeholder_window(child);
    }
    TAILQ_FOREACH(child, &(parent->floating_head), floating_windows) {
        open_placeholder_window(child);
    }

    xcb_flush(restore_conn);
}

/*
 * Kill the placeholder window, if placeholder refers to a placeholder window.
 * This function is called when manage.c puts a window into an existing
 * container. In order not to leak resources, we need to destroy the window and
 * all associated X11 objects (pixmap/gc).
 *
 */
bool restore_kill_placeholder(xcb_window_t placeholder) {
    placeholder_state *state;
    TAILQ_FOREACH(state, &state_head, state) {
        if (state->window != placeholder)
            continue;

        xcb_destroy_window(restore_conn, state->window);
        xcb_free_pixmap(restore_conn, state->pixmap);
        xcb_free_gc(restore_conn, state->gc);
        TAILQ_REMOVE(&state_head, state, state);
        free(state);
        DLOG("placeholder window 0x%08x destroyed.\n", placeholder);
        return true;
    }

    DLOG("0x%08x is not a placeholder window, ignoring.\n", placeholder);
    return false;
}

static void expose_event(xcb_expose_event_t *event) {
    placeholder_state *state;
    TAILQ_FOREACH(state, &state_head, state) {
        if (state->window != event->window)
            continue;

        DLOG("refreshing window 0x%08x contents (con %p)\n", state->window, state->con);

        /* Since we render to our pixmap on every change anyways, expose events
         * only tell us that the X server lost (parts of) the window contents. We
         * can handle that by copying the appropriate part from our pixmap to the
         * window. */
        xcb_copy_area(restore_conn, state->pixmap, state->window, state->gc,
                      event->x, event->y, event->x, event->y,
                      event->width, event->height);
        xcb_flush(restore_conn);
        return;
    }

    ELOG("Received ExposeEvent for unknown window 0x%08x\n", event->window);
}

/*
 * Window size has changed. Update the width/height, then recreate the back
 * buffer pixmap and the accompanying graphics context and force an immediate
 * re-rendering.
 *
 */
static void configure_notify(xcb_configure_notify_event_t *event) {
    placeholder_state *state;
    TAILQ_FOREACH(state, &state_head, state) {
        if (state->window != event->window)
            continue;

        DLOG("ConfigureNotify: window 0x%08x has now width=%d, height=%d (con %p)\n",
             state->window, event->width, event->height, state->con);

        state->rect.width = event->width;
        state->rect.height = event->height;

        xcb_free_pixmap(restore_conn, state->pixmap);
        xcb_free_gc(restore_conn, state->gc);

        state->pixmap = xcb_generate_id(restore_conn);
        xcb_create_pixmap(restore_conn, root_depth, state->pixmap,
                          state->window, state->rect.width, state->rect.height);
        state->gc = xcb_generate_id(restore_conn);
        xcb_create_gc(restore_conn, state->gc, state->pixmap, XCB_GC_GRAPHICS_EXPOSURES, (uint32_t[]){ 0 });

        update_placeholder_contents(state);
        xcb_copy_area(restore_conn, state->pixmap, state->window, state->gc,
                      0, 0, 0, 0, state->rect.width, state->rect.height);
        xcb_flush(restore_conn);
        return;
    }

    ELOG("Received ConfigureNotify for unknown window 0x%08x\n", event->window);
}

static void restore_handle_event(int type, xcb_generic_event_t *event) {
    switch (type) {
        case XCB_EXPOSE:
            expose_event((xcb_expose_event_t*)event);
            break;
        case XCB_CONFIGURE_NOTIFY:
            configure_notify((xcb_configure_notify_event_t*)event);
            break;
        default:
            DLOG("Received unhandled X11 event of type %d\n", type);
            break;
    }
}
