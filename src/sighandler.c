/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 * © 2009-2010 Jan-Erik Rediger
 *
 * sighandler.c: Interactive crash dialog upon SIGSEGV/SIGABRT/SIGFPE (offers
 *               to restart inplace).
 *
 */
#include "all.h"

#include <ev.h>
#include <iconv.h>
#include <signal.h>

#include <xcb/xcb_event.h>

#include <X11/keysym.h>

static xcb_gcontext_t pixmap_gc;
static xcb_pixmap_t pixmap;
static int raised_signal;

static char *crash_text[] = {
    "i3 just crashed.",
    "To debug this problem, either attach gdb now",
    "or press",
    "- 'e' to exit and get a core-dump,",
    "- 'r' to restart i3 in-place or",
    "- 'f' to forget the current layout and restart"
};
static int crash_text_longest = 5;

/*
 * Draw the window containing the info text
 *
 */
static int sig_draw_window(xcb_window_t win, int width, int height, int font_height) {
    /* re-draw the background */
    xcb_rectangle_t border = { 0, 0, width, height},
                    inner = { 2, 2, width - 4, height - 4};
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){ get_colorpixel("#FF0000") });
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){ get_colorpixel("#000000") });
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &inner);

    /* restore font color */
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){ get_colorpixel("#FFFFFF") });

    for (int i = 0; i < sizeof(crash_text) / sizeof(char*); i++) {
        draw_text(crash_text[i], strlen(crash_text[i]), false,
                pixmap, pixmap_gc, 8, 3 + (i - 1) * font_height);
    }

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, width, height);
    xcb_flush(conn);

    return 1;
}

/*
 * Handles keypresses of 'e' or 'r' to exit or restart i3
 *
 */
static int sig_handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    uint16_t state = event->state;

    /* Apparantly, after activating numlock once, the numlock modifier
     * stays turned on (use xev(1) to verify). So, to resolve useful
     * keysyms, we remove the numlock flag from the event state */
    state &= ~xcb_numlock_mask;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(keysyms, event, state);

    if (sym == 'e') {
        DLOG("User issued exit-command, raising error again.\n");
        raise(raised_signal);
        exit(1);
    }

    if (sym == 'r')
        i3_restart(false);

    if (sym == 'f')
        i3_restart(true);

    return 1;
}

/*
 * Opens the window we use for input/output and maps it
 *
 */
static xcb_window_t open_input_window(xcb_connection_t *conn, Rect screen_rect, uint32_t width, uint32_t height) {
    xcb_window_t win = xcb_generate_id(conn);

    uint32_t mask = 0;
    uint32_t values[2];

    mask |= XCB_CW_BACK_PIXEL;
    values[0] = 0;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[1] = 1;

    /* center each popup on the specified screen */
    uint32_t x = screen_rect.x + ((screen_rect.width / 2) - (width / 2)),
             y = screen_rect.y + ((screen_rect.height / 2) - (height / 2));

    xcb_create_window(conn,
                      XCB_COPY_FROM_PARENT,
                      win, /* the window id */
                      root, /* parent == root */
                      x, y, width, height, /* dimensions */
                      0, /* border = 0, we draw our own */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                      mask,
                      values);

    /* Map the window (= make it visible) */
    xcb_map_window(conn, win);

    return win;
}

/*
 * Handle signals
 * It creates a window asking the user to restart in-place
 * or exit to generate a core dump
 *
 */
void handle_signal(int sig, siginfo_t *info, void *data) {
    DLOG("i3 crashed. SIG: %d\n", sig);

    struct sigaction action;
    action.sa_handler = SIG_DFL;
    sigaction(sig, &action, NULL);
    raised_signal = sig;

    /* width and height of the popup window, so that the text fits in */
    int crash_text_num = sizeof(crash_text) / sizeof(char*);
    int height = 13 + (crash_text_num * config.font.height);

    /* calculate width for longest text */
    int text_len = strlen(crash_text[crash_text_longest]);
    xcb_char2b_t *longest_text = convert_utf8_to_ucs2(crash_text[crash_text_longest], &text_len);
    int font_width = predict_text_width((char *)longest_text, text_len, true);
    int width = font_width + 20;

    /* Open a popup window on each virtual screen */
    Output *screen;
    xcb_window_t win;
    TAILQ_FOREACH(screen, &outputs, outputs) {
        if (!screen->active)
            continue;
        win = open_input_window(conn, screen->rect, width, height);

        /* Create pixmap */
        pixmap = xcb_generate_id(conn);
        pixmap_gc = xcb_generate_id(conn);
        xcb_create_pixmap(conn, root_depth, pixmap, win, width, height);
        xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);

        /* Create graphics context */
        xcb_change_gc(conn, pixmap_gc, XCB_GC_FONT, (uint32_t[]){ config.font.id });

        /* Grab the keyboard to get all input */
        xcb_grab_keyboard(conn, false, win, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

        /* Grab the cursor inside the popup */
        xcb_grab_pointer(conn, false, win, XCB_NONE, XCB_GRAB_MODE_ASYNC,
                         XCB_GRAB_MODE_ASYNC, win, XCB_NONE, XCB_CURRENT_TIME);

        sig_draw_window(win, width, height, config.font.height);
        xcb_flush(conn);
    }

    xcb_generic_event_t *event;
    /* Yay, more own eventhandlers… */
    while ((event = xcb_wait_for_event(conn))) {
        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);
        if (type == XCB_KEY_PRESS) {
            sig_handle_key_press(NULL, conn, (xcb_key_press_event_t*)event);
        }
        free(event);
    }
}

/*
 * Setup signal handlers to safely handle SIGSEGV and SIGFPE
 *
 */
void setup_signal_handler() {
    struct sigaction action;

    action.sa_sigaction = handle_signal;
    action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGSEGV, &action, NULL) == -1 ||
        sigaction(SIGABRT, &action, NULL) == -1 ||
        sigaction(SIGFPE, &action, NULL) == -1)
        ELOG("Could not setup signal handler");
}
