/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 * src/xcb.c: Communicating with X
 *
 */
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <i3/ipc.h>
#include <ev.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>

#include "common.h"

/* We save the Atoms in an easy to access array, indexed by an enum */
#define NUM_ATOMS 3

enum {
    #define ATOM_DO(name) name,
    #include "xcb_atoms.def"
};

xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];
xcb_atom_t               atoms[NUM_ATOMS];

/* Variables, that are the same for all functions at all times */
xcb_connection_t *xcb_connection;
xcb_screen_t     *xcb_screens;
xcb_window_t     xcb_root;
xcb_font_t       xcb_font;

/* We need to cache some data to speed up text-width-prediction */
xcb_query_font_reply_t *font_info;
xcb_charinfo_t         *font_table;

/* These are only relevant for XKB, which we only need for grabbing modifiers */
Display          *xkb_dpy;
int              xkb_event_base;
int              mod_pressed;

/* Because the statusline is the same on all outputs, we have
 * global buffer to render it on */
xcb_gcontext_t   statusline_ctx;
xcb_pixmap_t     statusline_pm;
uint32_t         statusline_width;

/* Event-Watchers, to interact with the user */
ev_prepare *xcb_prep;
ev_check   *xcb_chk;
ev_io      *xcb_io;
ev_io      *xkb_io;

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
    statusline_width = predict_text_extents(text, glyph_count);

    xcb_free_pixmap(xcb_connection, statusline_pm);
    statusline_pm = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                               xcb_screens->root_depth,
                                                               statusline_pm,
                                                               xcb_root,
                                                               statusline_width,
                                                               font_height);

    draw_text(statusline_pm, statusline_ctx, 0, 0, text, glyph_count);

    xcb_generic_error_t *err;
    if ((err = xcb_request_check(xcb_connection, sl_pm_cookie)) != NULL) {
        printf("ERROR: Could not allocate statusline-buffer! XCB-error: %d\n", err->error_code);
        exit(EXIT_FAILURE);
    }
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
    xcb_generic_error_t *err;
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
        printf("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
        cookie = xcb_configure_window_checked(xcb_connection,
                                              walk->bar,
                                              mask,
                                              values);

        if ((err = xcb_request_check(xcb_connection, cookie)) != NULL) {
            printf("ERROR: Could not reconfigure window. XCB-errorcode: %d\n", err->error_code);
            exit(EXIT_FAILURE);
        }
        xcb_map_window(xcb_connection, walk->bar);
    }
}

/*
 * Handle a button-press-event (i.c. a mouse click on one of our bars).
 * We determine, wether the click occured on a ws-button or if the scroll-
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
        printf("Unknown Bar klicked!\n");
        return;
    }

    /* TODO: Move this to exern get_ws_for_output() */
    TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
        if (cur_ws->visible) {
            break;
        }
    }

    if (cur_ws == NULL) {
        printf("No Workspace active?\n");
        return;
    }

    int32_t x = event->event_x;

    printf("Got Button %d\n", event->detail);

    switch (event->detail) {
        case 1:
            /* Left Mousbutton. We determine, which button was clicked
             * and set cur_ws accordingly */
            TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
                printf("x = %d\n", x);
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

    char buffer[50];
    snprintf(buffer, 50, "%d", cur_ws->num);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_COMMAND, buffer);
}

/*
 * This function is called immediately bevor the main loop locks. We flush xcb
 * then (and only then)
 *
 */
void xcb_prep_cb(struct ev_loop *loop, ev_prepare *watcher, int revenst) {
    xcb_flush(xcb_connection);
}

/*
 * This function is called immediately after the main loop locks, so when one
 * of the watchers registered an event.
 * We check wether an X-Event arrived and handle it.
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
    int modstate;

    printf("Got XKB-Event!\n");

    while (XPending(xkb_dpy)) {
        XNextEvent(xkb_dpy, (XEvent*)&ev);

        if (ev.type != xkb_event_base) {
            printf("ERROR: No Xkb-Event!\n");
            continue;
        }

        if (ev.any.xkb_type != XkbStateNotify) {
            printf("ERROR: No State Notify!\n");
            continue;
        }

        unsigned int mods = ev.state.mods;
        modstate = mods & Mod4Mask;
    }

    if (modstate != mod_pressed) {
        if (modstate == 0) {
            printf("Mod4 got released!\n");
            hide_bars();
        } else {
            printf("Mod4 got pressed!\n");
            unhide_bars();
        }
        mod_pressed = modstate;
    }
}

/*
 * Initialize xcb and use the specified fontname for text-rendering
 *
 */
void init_xcb(char *fontname) {
    /* FIXME: xcb_connect leaks Memory */
    xcb_connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(xcb_connection)) {
        printf("Cannot open display\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to xcb\n");

    /* We have to request the atoms we need */
    #define ATOM_DO(name) atom_cookies[name] = xcb_intern_atom(xcb_connection, 0, strlen(#name), #name);
    #include "xcb_atoms.def"

    xcb_screens = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection)).data;
    xcb_root = xcb_screens->root;

    /* We load and allocate the font */
    xcb_font = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t open_font_cookie;
    open_font_cookie = xcb_open_font_checked(xcb_connection,
                                             xcb_font,
                                             strlen(fontname),
                                             fontname);

    xcb_generic_error_t *err = xcb_request_check(xcb_connection,
                                                 open_font_cookie);

    if (err != NULL) {
        printf("ERROR: Could not open font! XCB-Error-Code: %d\n", err->error_code);
        exit(EXIT_FAILURE);
    }

    /* We also need the fontheight to configure our bars accordingly */
    xcb_list_fonts_with_info_cookie_t font_info_cookie;
    font_info_cookie = xcb_list_fonts_with_info(xcb_connection,
                                                1,
                                                strlen(fontname),
                                                fontname);

    xcb_query_font_cookie_t query_font_cookie;
    query_font_cookie = xcb_query_font(xcb_connection,
                                       xcb_font);

    if (config.hide_on_modifier) {
        int xkb_major, xkb_minor, xkb_errbase, xkb_err;
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;

        xkb_dpy = XkbOpenDisplay(":0",
                                 &xkb_event_base,
                                 &xkb_errbase,
                                 &xkb_major,
                                 &xkb_minor,
                                 &xkb_err);

        if (xkb_dpy == NULL) {
            printf("ERROR: No XKB!\n");
            exit(EXIT_FAILURE);
        }

        if (fcntl(ConnectionNumber(xkb_dpy), F_SETFD, FD_CLOEXEC) == -1) {
            fprintf(stderr, "Could not set FD_CLOEXEC on xkbdpy\n");
            exit(EXIT_FAILURE);
        }

        int i1;
        if (!XkbQueryExtension(xkb_dpy, &i1, &xkb_event_base, &xkb_errbase, &xkb_major, &xkb_minor)) {
            printf("ERROR: XKB not supported by X-server!\n");
            exit(EXIT_FAILURE);
        }

        if (!XkbSelectEvents(xkb_dpy, XkbUseCoreKbd, XkbStateNotifyMask, XkbStateNotifyMask)) {
            printf("Could not grab Key!\n");
            exit(EXIT_FAILURE);
        }

        xkb_io = malloc(sizeof(ev_io));
        ev_io_init(xkb_io, &xkb_io_cb, ConnectionNumber(xkb_dpy), EV_READ);
        ev_io_start(main_loop, xkb_io);
        XFlush(xkb_dpy);
    }

    /* We draw the statusline to a seperate pixmap, because it looks the same on all bars and
     * this way, we can choose to crop it */
    statusline_ctx = xcb_generate_id(xcb_connection);
    uint32_t mask = XCB_GC_FOREGROUND |
                    XCB_GC_BACKGROUND |
                    XCB_GC_FONT;
    uint32_t vals[3] = { xcb_screens->white_pixel, xcb_screens->black_pixel, xcb_font };

    xcb_void_cookie_t sl_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                            statusline_ctx,
                                                            xcb_root,
                                                            mask,
                                                            vals);

    statusline_pm = xcb_generate_id(xcb_connection);

    /* The varios Watchers to communicate with xcb */
    xcb_io = malloc(sizeof(ev_io));
    xcb_prep = malloc(sizeof(ev_prepare));
    xcb_chk = malloc(sizeof(ev_check));

    ev_io_init(xcb_io, &xcb_io_cb, xcb_get_file_descriptor(xcb_connection), EV_READ);
    ev_prepare_init(xcb_prep, &xcb_prep_cb);
    ev_check_init(xcb_chk, &xcb_chk_cb);

    ev_io_start(main_loop, xcb_io);
    ev_prepare_start(main_loop, xcb_prep);
    ev_check_start(main_loop, xcb_chk);

    /* Now we get the atoms and save them in a nice data-structure */
    get_atoms();

    /* Now we calculate the font-height */
    xcb_list_fonts_with_info_reply_t *reply;
    reply = xcb_list_fonts_with_info_reply(xcb_connection,
                                           font_info_cookie,
                                           NULL);
    font_height = reply->font_ascent + reply->font_descent;
    FREE(reply);

    font_info = xcb_query_font_reply(xcb_connection,
                                     query_font_cookie,
                                     &err);

    if (err != NULL) {
        printf("ERROR: Could not query font! XCB-error: %d\n", err->error_code);
        exit(EXIT_FAILURE);
    }

    if (xcb_query_font_char_infos_length(font_info) == 0) {
        font_table = NULL;
    } else {
        font_table = xcb_query_font_char_infos(font_info);
    }

    printf("Calculated Font-height: %d\n", font_height);

    if((err = xcb_request_check(xcb_connection, sl_ctx_cookie)) != NULL) {
        printf("ERROR: Could not create context for statusline! XCB-error: %d\n", err->error_code);
        exit(EXIT_FAILURE);
    }
}

/*
 * Cleanup the xcb-stuff.
 * Called once, before the program terminates.
 *
 */
void clean_xcb() {
    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        destroy_window(walk);
    }
    FREE_SLIST(outputs, i3_output);

    xcb_disconnect(xcb_connection);

    ev_check_stop(main_loop, xcb_chk);
    ev_prepare_stop(main_loop, xcb_prep);
    ev_io_stop(main_loop, xcb_io);

    FREE(xcb_chk);
    FREE(xcb_prep);
    FREE(xcb_io);
}

/*
 * Get the earlier requested atoms and save them in the prepared data-structure
 *
 */
void get_atoms() {
    xcb_intern_atom_reply_t *reply;
    #define ATOM_DO(name) reply = xcb_intern_atom_reply(xcb_connection, atom_cookies[name], NULL); \
        if (reply == NULL) { \
            printf("ERROR: Could not get atom %s\n", #name); \
            exit(EXIT_FAILURE); \
        } \
        atoms[name] = reply->atom; \
        free(reply);

    #include "xcb_atoms.def"
    printf("Got Atoms\n");
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
 * Reconfigure all bars and create new for newly activated outputs
 *
 */
void reconfig_windows() {
    uint32_t mask;
    uint32_t values[5];

    xcb_generic_error_t *err;

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active) {
            /* If an output is not active, we destroy it's bar */
            /* FIXME: Maybe we rather want to unmap? */
            printf("Destroying window for output %s\n", walk->name);
            destroy_window(walk);
            continue;
        }
        if (walk->bar == XCB_NONE) {
            printf("Creating Window for output %s\n", walk->name);

            walk->bar = xcb_generate_id(xcb_connection);
            walk->buffer = xcb_generate_id(xcb_connection);
            mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
            /* Black background */
            values[0] = xcb_screens->black_pixel;
            /* If hide_on_modifier is set, i3 is not supposed to manage our bar-windows */
            values[1] = config.hide_on_modifier;
            /* The events we want to receive */
            values[2] = XCB_EVENT_MASK_EXPOSURE |
                        XCB_EVENT_MASK_BUTTON_PRESS;
            xcb_void_cookie_t win_cookie = xcb_create_window_checked(xcb_connection,
                                                                     xcb_screens->root_depth,
                                                                     walk->bar,
                                                                     xcb_root,
                                                                     walk->rect.x, walk->rect.y,
                                                                     walk->rect.w, font_height + 6,
                                                                     1,
                                                                     XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                                                     xcb_screens->root_visual,
                                                                     mask,
                                                                     values);

            xcb_void_cookie_t pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                                    xcb_screens->root_depth,
                                                                    walk->buffer,
                                                                    walk->bar,
                                                                    walk->rect.w,
                                                                    walk->rect.h);

            /* We want dock-windows (for now) */
            xcb_void_cookie_t prop_cookie = xcb_change_property(xcb_connection,
                                                                XCB_PROP_MODE_REPLACE,
                                                                walk->bar,
                                                                atoms[_NET_WM_WINDOW_TYPE],
                                                                atoms[ATOM],
                                                                32,
                                                                1,
                                                                (unsigned char*) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);
            /* We also want a graphics-context (the "canvas" on which we draw) */
            walk->bargc = xcb_generate_id(xcb_connection);
            mask = XCB_GC_FONT;
            values[0] = xcb_font;
            xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(xcb_connection,
                                                                walk->bargc,
                                                                walk->bar,
                                                                mask,
                                                                values);

            /* We finally map the bar (display it on screen) */
            xcb_void_cookie_t map_cookie = xcb_map_window_checked(xcb_connection, walk->bar);

            if ((err = xcb_request_check(xcb_connection, win_cookie)) != NULL) {
                printf("ERROR: Could not create Window. XCB-errorcode: %d\n", err->error_code);
                exit(EXIT_FAILURE);
            }

            if ((err = xcb_request_check(xcb_connection, pm_cookie)) != NULL) {
                printf("ERROR: Could not create Pixmap. XCB-errorcode: %d\n", err->error_code);
                exit(EXIT_FAILURE);
            }

            if ((err = xcb_request_check(xcb_connection, prop_cookie)) != NULL) {
                printf("ERROR: Could not set dock mode. XCB-errorcode: %d\n", err->error_code);
                exit(EXIT_FAILURE);
            }

            if ((err = xcb_request_check(xcb_connection, gc_cookie)) != NULL) {
                printf("ERROR: Could not create graphical context. XCB-errorcode: %d\n", err->error_code);
                exit(EXIT_FAILURE);
            }

            if ((err = xcb_request_check(xcb_connection, map_cookie)) != NULL) {
                printf("ERROR: Could not map window. XCB-errorcode: %d\n", err->error_code);
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
            printf("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
            xcb_void_cookie_t cfg_cookie = xcb_configure_window_checked(xcb_connection,
                                                                        walk->bar,
                                                                        mask,
                                                                        values);

            if ((err = xcb_request_check(xcb_connection, cfg_cookie)) != NULL) {
                printf("ERROR: Could not reconfigure window. XCB-errorcode: %d\n", err->error_code);
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
    printf("Drawing Bars...\n");
    int i = 0;

    refresh_statusline();

    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
        if (!outputs_walk->active) {
            printf("Output %s inactive, skipping...\n", outputs_walk->name);
            continue;
        }
        if (outputs_walk->bar == XCB_NONE) {
            reconfig_windows();
        }
        uint32_t color = get_colorpixel("000000");
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
            printf("Printing statusline!\n");

            xcb_copy_area(xcb_connection,
                          statusline_pm,
                          outputs_walk->buffer,
                          outputs_walk->bargc,
                          MAX(0, (int16_t)(statusline_width - outputs_walk->rect.w + 4)), 0,
                          MAX(0, (int16_t)(outputs_walk->rect.w - statusline_width - 4)), 3,
                          MIN(outputs_walk->rect.w - 4, statusline_width), font_height);
        }

        i3_ws *ws_walk;
        TAILQ_FOREACH(ws_walk, outputs_walk->workspaces, tailq) {
            printf("Drawing Button for WS %s at x = %d\n", ws_walk->name, i);
            uint32_t color = get_colorpixel("240000");
            if (ws_walk->visible) {
                color = get_colorpixel("480000");
            }
            if (ws_walk->urgent) {
                printf("WS %s is urgent!\n", ws_walk->name);
                color = get_colorpixel("002400");
                /* The urgent-hint should get noticed, so we unhide the bars shortly */
                unhide_bars();
            }
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_FOREGROUND,
                          &color);
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_BACKGROUND,
                          &color);
            xcb_rectangle_t rect = { i + 1, 1, ws_walk->name_width + 8, font_height + 4 };
            xcb_poly_fill_rectangle(xcb_connection,
                                    outputs_walk->buffer,
                                    outputs_walk->bargc,
                                    1,
                                    &rect);
            color = get_colorpixel("FFFFFF");
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          XCB_GC_FOREGROUND,
                          &color);
            xcb_image_text_16(xcb_connection,
                              ws_walk->name_glyphs,
                              outputs_walk->buffer,
                              outputs_walk->bargc,
                              i + 5, font_height + 1,
                              ws_walk->ucs2_name);
            i += 10 + ws_walk->name_width;
        }

        redraw_bars();

        i = 0;
    }
}

/*
 * Redraw the bars, i.e. simply copy the buffer to the barwindow
 *
 */
void redraw_bars() {
    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
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
