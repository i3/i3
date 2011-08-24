/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010-2011 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 * src/xcb.c: Communicating with X
 *
 */
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <i3/ipc.h>
#include <ev.h>
#include <errno.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>

#include "common.h"

#if defined(__APPLE__)

/*
 * Taken from FreeBSD
 * Returns a pointer to a new string which is a duplicate of the
 * string, but only copies at most n characters.
 *
 */
char *strndup(const char *str, size_t n) {
    size_t len;
    char *copy;

    for (len = 0; len < n && str[len]; len++)
        continue;

    if ((copy = malloc(len + 1)) == NULL)
        return (NULL);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return (copy);
}

#endif

/* We save the Atoms in an easy to access array, indexed by an enum */
enum {
    #define ATOM_DO(name) name,
    #include "xcb_atoms.def"
    NUM_ATOMS
};

xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];
xcb_atom_t               atoms[NUM_ATOMS];

/* Variables, that are the same for all functions at all times */
xcb_connection_t *xcb_connection;
xcb_screen_t     *xcb_screen;
xcb_window_t     xcb_root;
xcb_font_t       xcb_font;

/* We need to cache some data to speed up text-width-prediction */
xcb_query_font_reply_t *font_info;
int                    font_height;
xcb_charinfo_t         *font_table;

/* These are only relevant for XKB, which we only need for grabbing modifiers */
Display          *xkb_dpy;
int              xkb_event_base;
int              mod_pressed = 0;

/* Because the statusline is the same on all outputs, we have
 * global buffer to render it on */
xcb_gcontext_t   statusline_ctx;
xcb_gcontext_t   statusline_clear;
xcb_pixmap_t     statusline_pm;
uint32_t         statusline_width;

/* Event-Watchers, to interact with the user */
ev_prepare *xcb_prep;
ev_check   *xcb_chk;
ev_io      *xcb_io;
ev_io      *xkb_io;

/* The parsed colors */
struct xcb_colors_t {
    uint32_t bar_fg;
    uint32_t bar_bg;
    uint32_t active_ws_fg;
    uint32_t active_ws_bg;
    uint32_t inactive_ws_fg;
    uint32_t inactive_ws_bg;
    uint32_t urgent_ws_bg;
    uint32_t urgent_ws_fg;
    uint32_t focus_ws_bg;
    uint32_t focus_ws_fg;
};
struct xcb_colors_t colors;

/* We define xcb_request_failed as a macro to include the relevant line-number */
#define xcb_request_failed(cookie, err_msg) _xcb_request_failed(cookie, err_msg, __LINE__)
int _xcb_request_failed(xcb_void_cookie_t cookie, char *err_msg, int line) {
    xcb_generic_error_t *err;
    if ((err = xcb_request_check(xcb_connection, cookie)) != NULL) {
        fprintf(stderr, "[%s:%d] ERROR: %s. X Error Code: %d\n", __FILE__, line, err_msg, err->error_code);
        return err->error_code;
    }
    return 0;
}

/*
 * Predicts the length of text based on cached data.
 * The string has to be encoded in ucs2 and glyph_len has to be the length
 * of the string (in glyphs).
 *
 */
uint32_t predict_text_extents(xcb_char2b_t *text, uint32_t length) {
    /* If we don't have per-character data, return the maximum width */
    if (font_table == NULL) {
        return (font_info->max_bounds.character_width * length);
    }

    uint32_t width = 0;
    uint32_t i;

    for (i = 0; i < length; i++) {
        xcb_charinfo_t *info;
        int row = text[i].byte1;
        int col = text[i].byte2;

        if (row < font_info->min_byte1 || row > font_info->max_byte1 ||
            col < font_info->min_char_or_byte2 || col > font_info->max_char_or_byte2) {
            continue;
        }

        /* Don't you ask me, how this one works… */
        info = &font_table[((row - font_info->min_byte1) *
                            (font_info->max_char_or_byte2 - font_info->min_char_or_byte2 + 1)) +
                           (col - font_info->min_char_or_byte2)];

        if (info->character_width != 0 ||
            (info->right_side_bearing |
             info->left_side_bearing |
             info->ascent |
             info->descent) != 0) {
            width += info->character_width;
        }
    }

    return width;
}

/*
 * Draws text given in UCS-2-encoding to a given drawable and position
 *
 */
void draw_text(xcb_drawable_t drawable, xcb_gcontext_t ctx, int16_t x, int16_t y,
               xcb_char2b_t *text, uint32_t glyph_count) {
    int offset = 0;
    int16_t pos_x = x;
    int16_t font_ascent = font_info->font_ascent;

    while (glyph_count > 0) {
        uint8_t chunk_size = MIN(255, glyph_count);
        uint32_t chunk_width = predict_text_extents(text + offset, chunk_size);

        xcb_image_text_16(xcb_connection,
                          chunk_size,
                          drawable,
                          ctx,
                          pos_x, y + font_ascent,
                          text + offset);

        offset += chunk_size;
        pos_x += chunk_width;
        glyph_count -= chunk_size;
    }
}

/*
 * Converts a colorstring to a colorpixel as expected from xcb_change_gc.
 * s is assumed to be in the format "rrggbb"
 *
 */
uint32_t get_colorpixel(const char *s) {
    char strings[3][3] = { { s[0], s[1], '\0'} ,
                           { s[2], s[3], '\0'} ,
                           { s[4], s[5], '\0'} };
    uint8_t r = strtol(strings[0], NULL, 16);
    uint8_t g = strtol(strings[1], NULL, 16);
    uint8_t b = strtol(strings[2], NULL, 16);
    return (r << 16 | g << 8 | b);
}

/*
 * Redraws the statusline to the buffer
 *
 */
void refresh_statusline() {
    int glyph_count;

    if (statusline == NULL) {
        return;
    }

    xcb_char2b_t *text = (xcb_char2b_t*) convert_utf8_to_ucs2(statusline, &glyph_count);
    uint32_t old_statusline_width = statusline_width;
    statusline_width = predict_text_extents(text, glyph_count);
    /* If the statusline is bigger than our screen we need to make sure that
     * the pixmap provides enough space, so re-allocate if the width grew */
    if (statusline_width > xcb_screen->width_in_pixels &&
        statusline_width > old_statusline_width)
        realloc_sl_buffer();

    xcb_rectangle_t rect = { 0, 0, xcb_screen->width_in_pixels, font_height };
    xcb_poly_fill_rectangle(xcb_connection, statusline_pm, statusline_clear, 1, &rect);
    draw_text(statusline_pm, statusline_ctx, 0, 0, text, glyph_count);

    FREE(text);
}

/*
 * Hides all bars (unmaps them)
 *
 */
void hide_bars() {
    if (!config.hide_on_modifier) {
        return;
    }

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active) {
            continue;
        }
        xcb_unmap_window(xcb_connection, walk->bar);
    }
    stop_child();
}

/*
 * Unhides all bars (maps them)
 *
 */
void unhide_bars() {
    if (!config.hide_on_modifier) {
        return;
    }

    i3_output           *walk;
    xcb_void_cookie_t   cookie;
    uint32_t            mask;
    uint32_t            values[5];

    cont_child();

    SLIST_FOREACH(walk, outputs, slist) {
        if (walk->bar == XCB_NONE) {
            continue;
        }
        mask = XCB_CONFIG_WINDOW_X |
               XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH |
               XCB_CONFIG_WINDOW_HEIGHT |
               XCB_CONFIG_WINDOW_STACK_MODE;
        values[0] = walk->rect.x;
        values[1] = walk->rect.y + walk->rect.h - font_height - 6;
        values[2] = walk->rect.w;
        values[3] = font_height + 6;
        values[4] = XCB_STACK_MODE_ABOVE;
        DLOG("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
        cookie = xcb_configure_window_checked(xcb_connection,
                                              walk->bar,
                                              mask,
                                              values);

        if (xcb_request_failed(cookie, "Could not reconfigure window")) {
            exit(EXIT_FAILURE);
        }
        xcb_map_window(xcb_connection, walk->bar);
    }
}

/*
 * Parse the colors into a format that we can use
 *
 */
void init_colors(const struct xcb_color_strings_t *new_colors) {
#define PARSE_COLOR(name, def) \
    do { \
        colors.name = get_colorpixel(new_colors->name ? new_colors->name : def); \
    } while  (0)
    PARSE_COLOR(bar_fg, "FFFFFF");
    PARSE_COLOR(bar_bg, "000000");
    PARSE_COLOR(active_ws_fg, "FFFFFF");
    PARSE_COLOR(active_ws_bg, "480000");
    PARSE_COLOR(inactive_ws_fg, "FFFFFF");
    PARSE_COLOR(inactive_ws_bg, "240000");
    PARSE_COLOR(urgent_ws_fg, "FFFFFF");
    PARSE_COLOR(urgent_ws_bg, "002400");
    PARSE_COLOR(focus_ws_fg, "FFFFFF");
    PARSE_COLOR(focus_ws_bg, "480000");
#undef PARSE_COLOR
}

/*
 * Handle a button-press-event (i.e. a mouse click on one of our bars).
 * We determine, whether the click occured on a ws-button or if the scroll-
 * wheel was used and change the workspace appropriately
 *
 */
void handle_button(xcb_button_press_event_t *event) {
    i3_ws *cur_ws;

    /* Determine, which bar was clicked */
    i3_output *walk;
    xcb_window_t bar = event->event;
    SLIST_FOREACH(walk, outputs, slist) {
        if (walk->bar == bar) {
            break;
        }
    }

    if (walk == NULL) {
        DLOG("Unknown Bar klicked!\n");
        return;
    }

    /* TODO: Move this to extern get_ws_for_output() */
    TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
        if (cur_ws->visible) {
            break;
        }
    }

    if (cur_ws == NULL) {
        DLOG("No Workspace active?\n");
        return;
    }

    int32_t x = event->event_x;

    DLOG("Got Button %d\n", event->detail);

    switch (event->detail) {
        case 1:
            /* Left Mousbutton. We determine, which button was clicked
             * and set cur_ws accordingly */
            TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
                DLOG("x = %d\n", x);
                if (x < cur_ws->name_width + 10) {
                    break;
                }
                x -= cur_ws->name_width + 10;
            }
            if (cur_ws == NULL) {
                return;
            }
            break;
        case 4:
            /* Mouse wheel down. We select the next ws */
            if (cur_ws == TAILQ_FIRST(walk->workspaces)) {
                cur_ws = TAILQ_LAST(walk->workspaces, ws_head);
            } else {
                cur_ws = TAILQ_PREV(cur_ws, ws_head, tailq);
            }
            break;
        case 5:
            /* Mouse wheel up. We select the previos ws */
            if (cur_ws == TAILQ_LAST(walk->workspaces, ws_head)) {
                cur_ws = TAILQ_FIRST(walk->workspaces);
            } else {
                cur_ws = TAILQ_NEXT(cur_ws, tailq);
            }
            break;
    }

    const size_t len = strlen(cur_ws->name) + strlen("workspace \"\"") + 1;
    char buffer[len];
    snprintf(buffer, len, "workspace \"%s\"", cur_ws->name);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_COMMAND, buffer);
}

/*
 * This function is called immediately before the main loop locks. We flush xcb
 * then (and only then)
 *
 */
void xcb_prep_cb(struct ev_loop *loop, ev_prepare *watcher, int revents) {
    xcb_flush(xcb_connection);
}

/*
 * This function is called immediately after the main loop locks, so when one
 * of the watchers registered an event.
 * We check whether an X-Event arrived and handle it.
 *
 */
void xcb_chk_cb(struct ev_loop *loop, ev_check *watcher, int revents) {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(xcb_connection)) == NULL) {
        return;
    }

    switch (event->response_type & ~0x80) {
        case XCB_EXPOSE:
            /* Expose-events happen, when the window needs to be redrawn */
            redraw_bars();
            break;
        case XCB_BUTTON_PRESS:
            /* Button-press-events are mouse-buttons clicked on one of our bars */
            handle_button((xcb_button_press_event_t*) event);
            break;
    }
    FREE(event);
}

/*
 * Dummy Callback. We only need this, so that the Prepare- and Check-Watchers
 * are triggered
 *
 */
void xcb_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
}

/*
 * We need to bind to the modifier per XKB. Sadly, XCB does not implement this
 *
 */
void xkb_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    XkbEvent ev;
    int modstate = 0;

    DLOG("Got XKB-Event!\n");

    while (XPending(xkb_dpy)) {
        XNextEvent(xkb_dpy, (XEvent*)&ev);

        if (ev.type != xkb_event_base) {
            ELOG("No Xkb-Event!\n");
            continue;
        }

        if (ev.any.xkb_type != XkbStateNotify) {
            ELOG("No State Notify!\n");
            continue;
        }

        unsigned int mods = ev.state.mods;
        modstate = mods & Mod4Mask;
    }

    if (modstate != mod_pressed) {
        if (modstate == 0) {
            DLOG("Mod4 got released!\n");
            hide_bars();
        } else {
            DLOG("Mod4 got pressed!\n");
            unhide_bars();
        }
        mod_pressed = modstate;
    }
}

/*
 * Initialize xcb and use the specified fontname for text-rendering
 *
 */
char *init_xcb(char *fontname) {
    /* FIXME: xcb_connect leaks Memory */
    xcb_connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(xcb_connection)) {
        ELOG("Cannot open display\n");
        exit(EXIT_FAILURE);
    }
    DLOG("Connected to xcb\n");

    /* We have to request the atoms we need */
    #define ATOM_DO(name) atom_cookies[name] = xcb_intern_atom(xcb_connection, 0, strlen(#name), #name);
    #include "xcb_atoms.def"

    xcb_screen = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection)).data;
    xcb_root = xcb_screen->root;

    /* We load and allocate the font */
    xcb_font = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t open_font_cookie;
    open_font_cookie = xcb_open_font_checked(xcb_connection,
                                             xcb_font,
                                             strlen(fontname),
                                             fontname);

    /* We need to save info about the font, because we need the font's height and
     * information about the width of characters */
    xcb_query_font_cookie_t query_font_cookie;
    query_font_cookie = xcb_query_font(xcb_connection,
                                       xcb_font);

    /* To grab modifiers without blocking other applications from receiving key-events
     * involving that modifier, we sadly have to use xkb which is not yet fully supported
     * in xcb */
    if (config.hide_on_modifier) {
        int xkb_major, xkb_minor, xkb_errbase, xkb_err;
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;

        xkb_dpy = XkbOpenDisplay(NULL,
                                 &xkb_event_base,
                                 &xkb_errbase,
                                 &xkb_major,
                                 &xkb_minor,
                                 &xkb_err);

        if (xkb_dpy == NULL) {
            ELOG("No XKB!\n");
            exit(EXIT_FAILURE);
        }

        if (fcntl(ConnectionNumber(xkb_dpy), F_SETFD, FD_CLOEXEC) == -1) {
            ELOG("Could not set FD_CLOEXEC on xkbdpy: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        int i1;
        if (!XkbQueryExtension(xkb_dpy, &i1, &xkb_event_base, &xkb_errbase, &xkb_major, &xkb_minor)) {
            ELOG("XKB not supported by X-server!\n");
            exit(EXIT_FAILURE);
        }

        if (!XkbSelectEvents(xkb_dpy, XkbUseCoreKbd, XkbStateNotifyMask, XkbStateNotifyMask)) {
            ELOG("Could not grab Key!\n");
            exit(EXIT_FAILURE);
        }

        xkb_io = malloc(sizeof(ev_io));
        ev_io_init(xkb_io, &xkb_io_cb, ConnectionNumber(xkb_dpy), EV_READ);
        ev_io_start(main_loop, xkb_io);
        XFlush(xkb_dpy);
    }

    /* We draw the statusline to a seperate pixmap, because it looks the same on all bars and
     * this way, we can choose to crop it */
    uint32_t mask = XCB_GC_FOREGROUND;
    uint32_t vals[3] = { colors.bar_bg, colors.bar_bg, xcb_font };

    statusline_clear = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t clear_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                               statusline_clear,
                                                               xcb_root,
                                                               mask,
                                                               vals);

    mask |= XCB_GC_BACKGROUND | XCB_GC_FONT;
    vals[0] = colors.bar_fg;
    statusline_ctx = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                            statusline_ctx,
                                                            xcb_root,
                                                            mask,
                                                            vals);

    statusline_pm = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                               xcb_screen->root_depth,
                                                               statusline_pm,
                                                               xcb_root,
                                                               xcb_screen->width_in_pixels,
                                                               xcb_screen->height_in_pixels);


    /* The various Watchers to communicate with xcb */
    xcb_io = malloc(sizeof(ev_io));
    xcb_prep = malloc(sizeof(ev_prepare));
    xcb_chk = malloc(sizeof(ev_check));

    ev_io_init(xcb_io, &xcb_io_cb, xcb_get_file_descriptor(xcb_connection), EV_READ);
    ev_prepare_init(xcb_prep, &xcb_prep_cb);
    ev_check_init(xcb_chk, &xcb_chk_cb);

    ev_io_start(main_loop, xcb_io);
    ev_prepare_start(main_loop, xcb_prep);
    ev_check_start(main_loop, xcb_chk);

    /* Now we get the atoms and save them in a nice data structure */
    get_atoms();

    xcb_get_property_cookie_t path_cookie;
    path_cookie = xcb_get_property_unchecked(xcb_connection,
                                   0,
                                   xcb_root,
                                   atoms[I3_SOCKET_PATH],
                                   XCB_GET_PROPERTY_TYPE_ANY,
                                   0, PATH_MAX);

    /* We check, if i3 set its socket-path */
    xcb_get_property_reply_t *path_reply = xcb_get_property_reply(xcb_connection,
                                                                  path_cookie,
                                                                  NULL);
    char *path = NULL;
    if (path_reply) {
        int len = xcb_get_property_value_length(path_reply);
        if (len != 0) {
            path = strndup(xcb_get_property_value(path_reply), len);
        }
    }

    /* Now we save the font-infos */
    font_info = xcb_query_font_reply(xcb_connection,
                                     query_font_cookie,
                                     NULL);

    if (xcb_request_failed(open_font_cookie, "Could not open font")) {
        exit(EXIT_FAILURE);
    }

    font_height = font_info->font_ascent + font_info->font_descent;

    if (xcb_query_font_char_infos_length(font_info) == 0) {
        font_table = NULL;
    } else {
        font_table = xcb_query_font_char_infos(font_info);
    }

    DLOG("Calculated Font-height: %d\n", font_height);

    if (xcb_request_failed(sl_pm_cookie, "Could not allocate statusline-buffer") ||
        xcb_request_failed(clear_ctx_cookie, "Could not allocate statusline-buffer-clearcontext") ||
        xcb_request_failed(sl_ctx_cookie, "Could not allocate statusline-buffer-context")) {
        exit(EXIT_FAILURE);
    }

    return path;
}

/*
 * Cleanup the xcb-stuff.
 * Called once, before the program terminates.
 *
 */
void clean_xcb() {
    i3_output *o_walk;
    free_workspaces();
    SLIST_FOREACH(o_walk, outputs, slist) {
        destroy_window(o_walk);
        FREE(o_walk->workspaces);
        FREE(o_walk->name);
    }
    FREE_SLIST(outputs, i3_output);
    FREE(outputs);

    xcb_disconnect(xcb_connection);

    ev_check_stop(main_loop, xcb_chk);
    ev_prepare_stop(main_loop, xcb_prep);
    ev_io_stop(main_loop, xcb_io);

    FREE(xcb_chk);
    FREE(xcb_prep);
    FREE(xcb_io);
    FREE(font_info);
}

/*
 * Get the earlier requested atoms and save them in the prepared data structure
 *
 */
void get_atoms() {
    xcb_intern_atom_reply_t *reply;
    #define ATOM_DO(name) reply = xcb_intern_atom_reply(xcb_connection, atom_cookies[name], NULL); \
        if (reply == NULL) { \
            ELOG("Could not get atom %s\n", #name); \
            exit(EXIT_FAILURE); \
        } \
        atoms[name] = reply->atom; \
        free(reply);

    #include "xcb_atoms.def"
    DLOG("Got Atoms\n");
}

/*
 * Destroy the bar of the specified output
 *
 */
void destroy_window(i3_output *output) {
    if (output == NULL) {
        return;
    }
    if (output->bar == XCB_NONE) {
        return;
    }
    xcb_destroy_window(xcb_connection, output->bar);
    output->bar = XCB_NONE;
}

/*
 * Reallocate the statusline-buffer
 *
 */
void realloc_sl_buffer() {
    DLOG("Re-allocating statusline-buffer, statusline_width = %d, xcb_screen->width_in_pixels = %d\n",
         statusline_width, xcb_screen->width_in_pixels);
    xcb_free_pixmap(xcb_connection, statusline_pm);
    statusline_pm = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                               xcb_screen->root_depth,
                                                               statusline_pm,
                                                               xcb_root,
                                                               MAX(xcb_screen->width_in_pixels, statusline_width),
                                                               xcb_screen->height_in_pixels);

    uint32_t mask = XCB_GC_FOREGROUND;
    uint32_t vals[3] = { colors.bar_bg, colors.bar_bg, xcb_font };
    xcb_free_gc(xcb_connection, statusline_clear);
    statusline_clear = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t clear_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                               statusline_clear,
                                                               xcb_root,
                                                               mask,
                                                               vals);

    mask |= XCB_GC_BACKGROUND | XCB_GC_FONT;
    vals[0] = colors.bar_fg;
    statusline_ctx = xcb_generate_id(xcb_connection);
    xcb_free_gc(xcb_connection, statusline_ctx);
    xcb_void_cookie_t sl_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                            statusline_ctx,
                                                            xcb_root,
                                                            mask,
                                                            vals);

    if (xcb_request_failed(sl_pm_cookie, "Could not allocate statusline-buffer") ||
        xcb_request_failed(clear_ctx_cookie, "Could not allocate statusline-buffer-clearcontext") ||
        xcb_request_failed(sl_ctx_cookie, "Could not allocate statusline-buffer-context")) {
        exit(EXIT_FAILURE);
    }

}

/*
 * Reconfigure all bars and create new bars for recently activated outputs
 *
 */
void reconfig_windows() {
    uint32_t mask;
    uint32_t values[5];

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active) {
            /* If an output is not active, we destroy its bar */
            /* FIXME: Maybe we rather want to unmap? */
            DLOG("Destroying window for output %s\n", walk->name);
            destroy_window(walk);
            continue;
        }
        if (walk->bar == XCB_NONE) {
            DLOG("Creating Window for output %s\n", walk->name);

            walk->bar = xcb_generate_id(xcb_connection);
            walk->buffer = xcb_generate_id(xcb_connection);
            mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
            /* Black background */
            values[0] = colors.bar_bg;
            /* If hide_on_modifier is set, i3 is not supposed to manage our bar-windows */
            values[1] = config.hide_on_modifier;
            /* The events we want to receive */
            values[2] = XCB_EVENT_MASK_EXPOSURE;
            if (!config.disable_ws) {
                values[2] |= XCB_EVENT_MASK_BUTTON_PRESS;
            }
            xcb_void_cookie_t win_cookie = xcb_create_window_checked(xcb_connection,
                                                                     xcb_screen->root_depth,
                                                                     walk->bar,
                                                                     xcb_root,
                                                                     walk->rect.x, walk->rect.y + walk->rect.h - font_height - 6,
                                                                     walk->rect.w, font_height + 6,
                                                                     1,
                                                                     XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                                                     xcb_screen->root_visual,
                                                                     mask,
                                                                     values);

            /* The double-buffer we use to render stuff off-screen */
            xcb_void_cookie_t pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                                    xcb_screen->root_depth,
                                                                    walk->buffer,
                                                                    walk->bar,
                                                                    walk->rect.w,
                                                                    walk->rect.h);

            /* We want dock-windows (for now). When override_redirect is set, i3 is ignoring
             * this one */
            xcb_void_cookie_t dock_cookie = xcb_change_property(xcb_connection,
                                                                XCB_PROP_MODE_REPLACE,
                                                                walk->bar,
                                                                atoms[_NET_WM_WINDOW_TYPE],
                                                                XCB_ATOM_ATOM,
                                                                32,
                                                                1,
                                                                (unsigned char*) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);

            /* We need to tell i3, where to reserve space for i3bar */
            /* left, right, top, bottom, left_start_y, left_end_y,
             * right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x,
             * bottom_end_x */
            /* A local struct to save the strut_partial property */
            struct {
                uint32_t left;
                uint32_t right;
                uint32_t top;
                uint32_t bottom;
                uint32_t left_start_y;
                uint32_t left_end_y;
                uint32_t right_start_y;
                uint32_t right_end_y;
                uint32_t top_start_x;
                uint32_t top_end_x;
                uint32_t bottom_start_x;
                uint32_t bottom_end_x;
            } __attribute__((__packed__)) strut_partial = {0,};
            switch (config.dockpos) {
                case DOCKPOS_NONE:
                    break;
                case DOCKPOS_TOP:
                    strut_partial.top = font_height + 6;
                    strut_partial.top_start_x = walk->rect.x;
                    strut_partial.top_end_x = walk->rect.x + walk->rect.w;
                    break;
                case DOCKPOS_BOT:
                    strut_partial.bottom = font_height + 6;
                    strut_partial.bottom_start_x = walk->rect.x;
                    strut_partial.bottom_end_x = walk->rect.x + walk->rect.w;
                    break;
            }
            xcb_void_cookie_t strut_cookie = xcb_change_property(xcb_connection,
                                                                 XCB_PROP_MODE_REPLACE,
                                                                 walk->bar,
                                                                 atoms[_NET_WM_STRUT_PARTIAL],
                                                                 XCB_ATOM_CARDINAL,
                                                                 32,
                                                                 12,
                                                                 &strut_partial);

            /* We also want a graphics-context for the bars (it defines the properties
             * with which we draw to them) */
            walk->bargc = xcb_generate_id(xcb_connection);
            mask = XCB_GC_FONT;
            values[0] = xcb_font;
            xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(xcb_connection,
                                                                walk->bargc,
                                                                walk->bar,
                                                                mask,
                                                                values);

            /* We finally map the bar (display it on screen), unless the modifier-switch is on */
            xcb_void_cookie_t map_cookie;
            if (!config.hide_on_modifier) {
                map_cookie = xcb_map_window_checked(xcb_connection, walk->bar);
            }

            if (xcb_request_failed(win_cookie,   "Could not create window") ||
                xcb_request_failed(pm_cookie,    "Could not create pixmap") ||
                xcb_request_failed(dock_cookie,  "Could not set dock mode") ||
                xcb_request_failed(strut_cookie, "Could not set strut")     ||
                xcb_request_failed(gc_cookie,    "Could not create graphical context") ||
                (!config.hide_on_modifier && xcb_request_failed(map_cookie, "Could not map window"))) {
                exit(EXIT_FAILURE);
            }
        } else {
            /* We already have a bar, so we just reconfigure it */
            mask = XCB_CONFIG_WINDOW_X |
                   XCB_CONFIG_WINDOW_Y |
                   XCB_CONFIG_WINDOW_WIDTH |
                   XCB_CONFIG_WINDOW_HEIGHT |
                   XCB_CONFIG_WINDOW_STACK_MODE;
            values[0] = walk->rect.x;
            values[1] = walk->rect.y + walk->rect.h - font_height - 6;
            values[2] = walk->rect.w;
            values[3] = font_height + 6;
            values[4] = XCB_STACK_MODE_ABOVE;

            DLOG("Destroying buffer for output %s", walk->name);
            xcb_free_pixmap(xcb_connection, walk->buffer);

            DLOG("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
            xcb_void_cookie_t cfg_cookie = xcb_configure_window_checked(xcb_connection,
                                                                        walk->bar,
                                                                        mask,
                                                                        values);

            DLOG("Recreating buffer for output %s", walk->name);
            xcb_void_cookie_t pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                                    xcb_screen->root_depth,
                                                                    walk->buffer,
                                                                    walk->bar,
                                                                    walk->rect.w,
                                                                    walk->rect.h);

            if (xcb_request_failed(cfg_cookie, "Could not reconfigure window")) {
                exit(EXIT_FAILURE);
            }
            if (xcb_request_failed(pm_cookie,  "Could not create pixmap")) {
                exit(EXIT_FAILURE);
            }
        }
    }
}

/*
 * Render the bars, with buttons and statusline
 *
 */
void draw_bars() {
    DLOG("Drawing Bars...\n");
    int i = 0;

    refresh_statusline();

    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
        if (!outputs_walk->active) {
            DLOG("Output %s inactive, skipping...\n", outputs_walk->name);
            continue;
        }
        if (outputs_walk->bar == XCB_NONE) {
            /* Oh shit, an active output without an own bar. Create it now! */
            reconfig_windows();
        }
        /* First things first: clear the backbuffer */
        uint32_t color = colors.bar_bg;
        xcb_change_gc(xcb_connection,
                      outputs_walk->bargc,
                      XCB_GC_FOREGROUND,
                      &color);
        xcb_rectangle_t rect = { 0, 0, outputs_walk->rect.w, font_height + 6 };
        xcb_poly_fill_rectangle(xcb_connection,
                                outputs_walk->buffer,
                                outputs_walk->bargc,
                                1,
                                &rect);

        if (statusline != NULL) {
            DLOG("Printing statusline!\n");

            /* Luckily we already prepared a seperate pixmap containing the rendered
             * statusline, we just have to copy the relevant parts to the relevant
             * position */
            xcb_copy_area(xcb_connection,
                          statusline_pm,
                          outputs_walk->buffer,
                          outputs_walk->bargc,
                          MAX(0, (int16_t)(statusline_width - outputs_walk->rect.w + 4)), 0,
                          MAX(0, (int16_t)(outputs_walk->rect.w - statusline_width - 4)), 3,
                          MIN(outputs_walk->rect.w - 4, statusline_width), font_height);
        }

        if (config.disable_ws) {
            continue;
        }

        i3_ws *ws_walk;
        TAILQ_FOREACH(ws_walk, outputs_walk->workspaces, tailq) {
            DLOG("Drawing Button for WS %s at x = %d\n", ws_walk->name, i);
            uint32_t fg_color = colors.inactive_ws_fg;
            uint32_t bg_color = colors.inactive_ws_bg;
            if (ws_walk->visible) {
                if (!ws_walk->focused) {
                    fg_color = colors.active_ws_fg;
                    bg_color = colors.active_ws_bg;
                } else {
                    fg_color = colors.focus_ws_fg;
                    bg_color = colors.focus_ws_bg;
                }
            }
            if (ws_walk->urgent) {
                DLOG("WS %s is urgent!\n", ws_walk->name);
                fg_color = colors.urgent_ws_fg;
                bg_color = colors.urgent_ws_bg;
                /* The urgent-hint should get noticed, so we unhide the bars shortly */
                unhide_bars();
            }
            uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
            uint32_t vals[] = { bg_color, bg_color };
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          mask,
                          vals);
            xcb_rectangle_t rect = { i + 1, 1, ws_walk->name_width + 8, font_height + 4 };
            xcb_poly_fill_rectangle(xcb_connection,
                                    outputs_walk->buffer,
                                    outputs_walk->bargc,
                                    1,
                                    &rect);
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_FOREGROUND,
                          &fg_color);
            xcb_image_text_16(xcb_connection,
                              ws_walk->name_glyphs,
                              outputs_walk->buffer,
                              outputs_walk->bargc,
                              i + 5, font_info->font_ascent + 2,
                              ws_walk->ucs2_name);
            i += 10 + ws_walk->name_width;
        }

        i = 0;
    }

    redraw_bars();
}

/*
 * Redraw the bars, i.e. simply copy the buffer to the barwindow
 *
 */
void redraw_bars() {
    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
        if (!outputs_walk->active) {
            continue;
        }
        xcb_copy_area(xcb_connection,
                      outputs_walk->buffer,
                      outputs_walk->bar,
                      outputs_walk->bargc,
                      0, 0,
                      0, 0,
                      outputs_walk->rect.w,
                      outputs_walk->rect.h);
        xcb_flush(xcb_connection);
    }
}
