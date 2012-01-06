/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-config-wizard: Program to convert configs using keycodes to configs using
 *                   keysyms.
 *
 */
#include <ev.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glob.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

/* We need SYSCONFDIR for the path to the keycode config template, so raise an
 * error if it’s not defined for whatever reason */
#ifndef SYSCONFDIR
#error "SYSCONFDIR not defined"
#endif

#define FREE(pointer) do { \
    if (pointer != NULL) { \
        free(pointer); \
        pointer = NULL; \
    } \
} \
while (0)

#include "xcb.h"
#include "libi3.h"

enum { STEP_WELCOME, STEP_GENERATE } current_step = STEP_WELCOME;
enum { MOD_Mod1, MOD_Mod4 } modifier = MOD_Mod4;

static char *config_path;
static uint32_t xcb_numlock_mask;
xcb_connection_t *conn;
static xcb_get_modifier_mapping_reply_t *modmap_reply;
static i3Font font;
static i3Font bold_font;
static char *socket_path;
static xcb_window_t win;
static xcb_pixmap_t pixmap;
static xcb_gcontext_t pixmap_gc;
static xcb_key_symbols_t *symbols;
xcb_window_t root;
Display *dpy;

char *rewrite_binding(const char *bindingline);
static void finish();

/*
 * This function resolves ~ in pathnames.
 * It may resolve wildcards in the first part of the path, but if no match
 * or multiple matches are found, it just returns a copy of path as given.
 *
 */
static char *resolve_tilde(const char *path) {
    static glob_t globbuf;
    char *head, *tail, *result;

    tail = strchr(path, '/');
    head = strndup(path, tail ? tail - path : strlen(path));

    int res = glob(head, GLOB_TILDE, NULL, &globbuf);
    free(head);
    /* no match, or many wildcard matches are bad */
    if (res == GLOB_NOMATCH || globbuf.gl_pathc != 1)
        result = strdup(path);
    else if (res != 0) {
        err(1, "glob() failed");
    } else {
        head = globbuf.gl_pathv[0];
        result = calloc(1, strlen(head) + (tail ? strlen(tail) : 0) + 1);
        strncpy(result, head, strlen(head));
        if (tail)
            strncat(result, tail, strlen(tail));
    }
    globfree(&globbuf);

    return result;
}

/*
 * Handles expose events, that is, draws the window contents.
 *
 */
static int handle_expose() {
    /* re-draw the background */
    xcb_rectangle_t border = {0, 0, 300, (15 * font.height) + 8};
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){ get_colorpixel("#000000") });
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);

    set_font(&font);

#define txt(x, row, text) \
    draw_text(text, strlen(text), false, pixmap, pixmap_gc,\
            x, (row - 1) * font.height + 4, 300 - x * 2)

    if (current_step == STEP_WELCOME) {
        /* restore font color */
        set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));

        txt(10, 2, "You have not configured i3 yet.");
        txt(10, 3, "Do you want me to generate ~/.i3/config?");
        txt(85, 5, "Yes, generate ~/.i3/config");
        txt(85, 7, "No, I will use the defaults");

        /* green */
        set_font_colors(pixmap_gc, get_colorpixel("#00FF00"), get_colorpixel("#000000"));
        txt(25, 5, "<Enter>");

        /* red */
        set_font_colors(pixmap_gc, get_colorpixel("#FF0000"), get_colorpixel("#000000"));
        txt(31, 7, "<ESC>");
    }

    if (current_step == STEP_GENERATE) {
        set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));

        txt(10, 2, "Please choose either:");
        txt(85, 4, "Win as default modifier");
        txt(85, 5, "Alt as default modifier");
        txt(10, 7, "Afterwards, press");
        txt(85, 9, "to write ~/.i3/config");
        txt(85, 10, "to abort");

        /* the not-selected modifier */
        if (modifier == MOD_Mod4)
            txt(31, 5, "<Alt>");
        else txt(31, 4, "<Win>");

        /* the selected modifier */
        set_font(&bold_font);
        set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));
        if (modifier == MOD_Mod4)
            txt(31, 4, "<Win>");
        else txt(31, 5, "<Alt>");

        /* green */
        set_font(&font);
        set_font_colors(pixmap_gc, get_colorpixel("#00FF00"), get_colorpixel("#000000"));
        txt(25, 9, "<Enter>");

        /* red */
        set_font_colors(pixmap_gc, get_colorpixel("#FF0000"), get_colorpixel("#000000"));
        txt(31, 10, "<ESC>");
    }

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, /* */ 500, 500);
    xcb_flush(conn);

    return 1;
}

static int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    printf("Keypress %d, state raw = %d\n", event->detail, event->state);

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     * button masks are filtered out */
    state_filtered &= 0xFF;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, state_filtered);

    printf("sym = %c (%d)\n", sym, sym);

    if (sym == XK_Return || sym == XK_KP_Enter) {
        if (current_step == STEP_WELCOME) {
            current_step = STEP_GENERATE;
            /* Set window title */
            xcb_change_property(conn,
                XCB_PROP_MODE_REPLACE,
                win,
                A__NET_WM_NAME,
                A_UTF8_STRING,
                8,
                strlen("i3: generate config"),
                "i3: generate config");
            xcb_flush(conn);
        }
        else finish();
    }

    /* cancel any time */
    if (sym == XK_Escape)
        exit(0);

    /* Check if this is Mod1 or Mod4. The modmap contains Shift, Lock, Control,
     * Mod1, Mod2, Mod3, Mod4, Mod5 (in that order) */
    xcb_keycode_t *modmap = xcb_get_modifier_mapping_keycodes(modmap_reply);
    /* Mod1? */
    int mask = 3;
    for (int i = 0; i < modmap_reply->keycodes_per_modifier; i++) {
        xcb_keycode_t code = modmap[(mask * modmap_reply->keycodes_per_modifier) + i];
        if (code == XCB_NONE)
            continue;
        printf("Modifier keycode for Mod1: 0x%02x\n", code);
        if (code == event->detail) {
            modifier = MOD_Mod1;
            printf("This is Mod1!\n");
        }
    }

    /* Mod4? */
    mask = 6;
    for (int i = 0; i < modmap_reply->keycodes_per_modifier; i++) {
        xcb_keycode_t code = modmap[(mask * modmap_reply->keycodes_per_modifier) + i];
        if (code == XCB_NONE)
            continue;
        printf("Modifier keycode for Mod4: 0x%02x\n", code);
        if (code == event->detail) {
            modifier = MOD_Mod4;
            printf("This is Mod4!\n");
        }
    }

    handle_expose();
    return 1;
}

/*
 * Handle button presses to make clicking on "<win>" and "<alt>" work
 *
 */
static void handle_button_press(xcb_button_press_event_t* event) {
    if (current_step != STEP_GENERATE)
        return;

    if (event->event_x >= 32 && event->event_x <= 68 &&
        event->event_y >= 45 && event->event_y <= 54) {
        modifier = MOD_Mod4;
        handle_expose();
    }

    if (event->event_x >= 32 && event->event_x <= 68 &&
        event->event_y >= 56 && event->event_y <= 70) {
        modifier = MOD_Mod1;
        handle_expose();
    }

    return;
}

/*
 * Creates the config file and tells i3 to reload.
 *
 */
static void finish() {
    printf("creating \"%s\"...\n", config_path);

    if (!(dpy = XOpenDisplay(NULL)))
        errx(1, "Could not connect to X11");

    FILE *kc_config = fopen(SYSCONFDIR "/i3/config.keycodes", "r");
    if (kc_config == NULL)
        err(1, "Could not open input file \"%s\"", SYSCONFDIR "/i3/config.keycodes");

    FILE *ks_config = fopen(config_path, "w");
    if (ks_config == NULL)
        err(1, "Could not open output config file \"%s\"", config_path);

    char *line = NULL;
    size_t len = 0;
#if !defined(__APPLE__) && (!defined(__FreeBSD__) || __FreeBSD_version >= 800000)
    ssize_t read;
#endif
    bool head_of_file = true;

    /* write a header about auto-generation to the output file */
    fputs("# This file has been auto-generated by i3-config-wizard(1).\n", ks_config);
    fputs("# It will not be overwritten, so edit it as you like.\n", ks_config);
    fputs("#\n", ks_config);
    fputs("# Should you change your keyboard layout somewhen, delete\n", ks_config);
    fputs("# this file and re-run i3-config-wizard(1).\n", ks_config);
    fputs("#\n", ks_config);

#if defined(__APPLE__) || (defined(__FreeBSD__) && __FreeBSD_version < 800000)
    while ((line = fgetln(kc_config, &len)) != NULL) {
#else
    while ((read = getline(&line, &len, kc_config)) != -1) {
#endif
        /* skip the warning block at the beginning of the input file */
        if (head_of_file &&
            strncmp("# WARNING", line, strlen("# WARNING")) == 0)
            continue;

        head_of_file = false;

        /* Skip leading whitespace */
        char *walk = line;
        while (isspace(*walk) && walk < (line + len))
            walk++;

        /* Set the modifier the user chose */
        if (strncmp(walk, "set $mod ", strlen("set $mod ")) == 0) {
            if (modifier == MOD_Mod1)
                fputs("set $mod Mod1\n", ks_config);
            else fputs("set $mod Mod4\n", ks_config);
            continue;
        }

        /* Check for 'bindcode'. If it’s not a bindcode line, we
         * just copy it to the output file */
        if (strncmp(walk, "bindcode", strlen("bindcode")) != 0) {
            fputs(line, ks_config);
            continue;
        }
        char *result = rewrite_binding(walk);
        fputs(result, ks_config);
        free(result);
    }

    /* sync to do our best in order to have the file really stored on disk */
    fflush(ks_config);
    fsync(fileno(ks_config));

    free(line);
    fclose(kc_config);
    fclose(ks_config);

    /* tell i3 to reload the config file */
    int sockfd = ipc_connect(socket_path);
    ipc_send_message(sockfd, strlen("reload"), 0, (uint8_t*)"reload");
    close(sockfd);

    exit(0);
}

int main(int argc, char *argv[]) {
    config_path = resolve_tilde("~/.i3/config");
    socket_path = getenv("I3SOCK");
    char *pattern = "-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
    char *patternbold = "-misc-fixed-bold-r-normal--13-120-75-75-C-70-iso10646-1";
    int o, option_index = 0;

    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {"limit", required_argument, 0, 'l'},
        {"prompt", required_argument, 0, 'P'},
        {"prefix", required_argument, 0, 'p'},
        {"font", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    char *options_string = "s:vh";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        switch (o) {
            case 's':
                FREE(socket_path);
                socket_path = strdup(optarg);
                break;
            case 'v':
                printf("i3-config-wizard " I3_VERSION "\n");
                return 0;
            case 'h':
                printf("i3-config-wizard " I3_VERSION "\n");
                printf("i3-config-wizard [-s <socket>] [-v]\n");
                return 0;
        }
    }

    /* Check if the destination config file does not exist but the path is
     * writable. If not, exit now, this program is not useful in that case. */
    struct stat stbuf;
    if (stat(config_path, &stbuf) == 0) {
        printf("The config file \"%s\" already exists. Exiting.\n", config_path);
        return 0;
    }

    /* Create ~/.i3 if it does not yet exist */
    char *config_dir = resolve_tilde("~/.i3");
    if (stat(config_dir, &stbuf) != 0)
        if (mkdir(config_dir, 0755) == -1)
            err(1, "mkdir(%s) failed", config_dir);
    free(config_dir);

    int fd;
    if ((fd = open(config_path, O_CREAT | O_RDWR, 0644)) == -1) {
        printf("Cannot open file \"%s\" for writing: %s. Exiting.\n", config_path, strerror(errno));
        return 0;
    }
    close(fd);
    unlink(config_path);

    if (socket_path == NULL)
        socket_path = root_atom_contents("I3_SOCKET_PATH");

    if (socket_path == NULL)
        socket_path = "/tmp/i3-ipc.sock";

    int screens;
    if ((conn = xcb_connect(NULL, &screens)) == NULL ||
        xcb_connection_has_error(conn))
        errx(1, "Cannot open display\n");

    xcb_get_modifier_mapping_cookie_t modmap_cookie;
    modmap_cookie = xcb_get_modifier_mapping(conn);
    symbols = xcb_key_symbols_alloc(conn);

    /* Place requests for the atoms we need as soon as possible */
    #define xmacro(atom) \
        xcb_intern_atom_cookie_t atom ## _cookie = xcb_intern_atom(conn, 0, strlen(#atom), #atom);
    #include "atoms.xmacro"
    #undef xmacro

    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screens);
    root = root_screen->root;

    if (!(modmap_reply = xcb_get_modifier_mapping_reply(conn, modmap_cookie, NULL)))
        errx(EXIT_FAILURE, "Could not get modifier mapping\n");

    xcb_numlock_mask = get_mod_mask_for(XCB_NUM_LOCK, symbols, modmap_reply);

    font = load_font(pattern, true);
    bold_font = load_font(patternbold, true);

    /* Open an input window */
    win = xcb_generate_id(conn);
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        win, /* the window id */
        root, /* parent == root */
        490, 297, 300, 205, /* dimensions */
        0, /* X11 border = 0, we draw our own */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        (uint32_t[]){
            0, /* back pixel: black */
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_BUTTON_PRESS
        });

    /* Map the window (make it visible) */
    xcb_map_window(conn, win);

    /* Setup NetWM atoms */
    #define xmacro(name) \
        do { \
            xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, name ## _cookie, NULL); \
            if (!reply) \
                errx(EXIT_FAILURE, "Could not get atom " # name "\n"); \
            \
            A_ ## name = reply->atom; \
            free(reply); \
        } while (0);
    #include "atoms.xmacro"
    #undef xmacro

    /* Set dock mode */
    xcb_change_property(conn,
        XCB_PROP_MODE_REPLACE,
        win,
        A__NET_WM_WINDOW_TYPE,
        A_ATOM,
        32,
        1,
        (unsigned char*) &A__NET_WM_WINDOW_TYPE_DIALOG);

    /* Set window title */
    xcb_change_property(conn,
        XCB_PROP_MODE_REPLACE,
        win,
        A__NET_WM_NAME,
        A_UTF8_STRING,
        8,
        strlen("i3: first configuration"),
        "i3: first configuration");

    /* Create pixmap */
    pixmap = xcb_generate_id(conn);
    pixmap_gc = xcb_generate_id(conn);
    xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, 500, 500);
    xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);

    /* Grab the keyboard to get all input */
    xcb_flush(conn);

    /* Try (repeatedly, if necessary) to grab the keyboard. We might not
     * get the keyboard at the first attempt because of the keybinding
     * still being active when started via a wm’s keybinding. */
    xcb_grab_keyboard_cookie_t cookie;
    xcb_grab_keyboard_reply_t *reply = NULL;

    int count = 0;
    while ((reply == NULL || reply->status != XCB_GRAB_STATUS_SUCCESS) && (count++ < 500)) {
        cookie = xcb_grab_keyboard(conn, false, win, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        reply = xcb_grab_keyboard_reply(conn, cookie, NULL);
        usleep(1000);
    }

    if (reply->status != XCB_GRAB_STATUS_SUCCESS) {
        fprintf(stderr, "Could not grab keyboard, status = %d\n", reply->status);
        exit(-1);
    }

    xcb_flush(conn);

    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            fprintf(stderr, "X11 Error received! sequence %x\n", event->sequence);
            continue;
        }

        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);

        switch (type) {
            case XCB_KEY_PRESS:
                handle_key_press(NULL, conn, (xcb_key_press_event_t*)event);
                break;

            /* TODO: handle mappingnotify */

            case XCB_BUTTON_PRESS:
                handle_button_press((xcb_button_press_event_t*)event);
                break;

            case XCB_EXPOSE:
                handle_expose();
                break;
        }

        free(event);
    }

    return 0;
}
