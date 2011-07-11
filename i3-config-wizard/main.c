/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * i3-config-wizard: Program to convert configs using keycodes to configs using
 * keysyms.
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
#include "ipc.h"

enum { STEP_WELCOME, STEP_GENERATE } current_step = STEP_WELCOME;
enum { MOD_ALT, MOD_SUPER } modifier = MOD_SUPER;

static char *config_path;
static xcb_connection_t *conn;
static uint32_t font_id;
static uint32_t font_bold_id;
static char *socket_path;
static int font_height;
static int font_bold_height;
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
 * Try to get the socket path from X11 and return NULL if it doesn’t work.
 * As i3-msg is a short-running tool, we don’t bother with cleaning up the
 * connection and leave it up to the operating system on exit.
 *
 */
static char *socket_path_from_x11() {
    xcb_connection_t *conn;
    int screen;
    if ((conn = xcb_connect(NULL, &screen)) == NULL ||
        xcb_connection_has_error(conn))
        return NULL;
    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screen);
    xcb_window_t root = root_screen->root;

    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;

    atom_cookie = xcb_intern_atom(conn, 0, strlen("I3_SOCKET_PATH"), "I3_SOCKET_PATH");
    atom_reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (atom_reply == NULL)
        return NULL;

    xcb_get_property_cookie_t prop_cookie;
    xcb_get_property_reply_t *prop_reply;
    prop_cookie = xcb_get_property_unchecked(conn, false, root, atom_reply->atom,
                                             XCB_GET_PROPERTY_TYPE_ANY, 0, PATH_MAX);
    prop_reply = xcb_get_property_reply(conn, prop_cookie, NULL);
    if (prop_reply == NULL || xcb_get_property_value_length(prop_reply) == 0)
        return NULL;
    if (asprintf(&socket_path, "%.*s", xcb_get_property_value_length(prop_reply),
                 (char*)xcb_get_property_value(prop_reply)) == -1)
        return NULL;
    return socket_path;
}

/*
 * Handles expose events, that is, draws the window contents.
 *
 */
static int handle_expose() {
    /* re-draw the background */
    xcb_rectangle_t border = {0, 0, 300, (15*font_height) + 8},
                    inner = {2, 2, 296, (15*font_height) + 8 - 4};
    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#285577"));
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);
    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &inner);

    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FONT, font_id);

#define txt(x, row, text) xcb_image_text_8(conn, strlen(text), pixmap, pixmap_gc, x, (row * font_height) + 2, text)

    if (current_step == STEP_WELCOME) {
        /* restore font color */
        xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#FFFFFF"));

        txt(10, 1, "i3: first configuration");
        txt(10, 4, "You have not configured i3 yet.");
        txt(10, 5, "Do you want me to generate ~/.i3/config?");
        txt(85, 8, "Yes, generate ~/.i3/config");
        txt(85, 10, "No, I will use the defaults");

        /* green */
        xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#00FF00"));
        txt(25, 8, "<Enter>");

        /* red */
        xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#FF0000"));
        txt(31, 10, "<ESC>");
    }

    if (current_step == STEP_GENERATE) {
        xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#FFFFFF"));

        txt(10, 1, "i3: generate config");

        txt(10, 4, "Please choose either:");
        txt(85, 6, "Win as default modifier");
        txt(85, 7, "Alt as default modifier");
        txt(10, 9, "Afterwards, press");
        txt(85, 11, "to write ~/.i3/config");
        txt(85, 12, "to abort");

        /* the not-selected modifier */
        if (modifier == MOD_SUPER)
            txt(31, 7, "<Alt>");
        else txt(31, 6, "<Win>");

        /* the selected modifier */
        xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FONT, font_bold_id);
        if (modifier == MOD_SUPER)
            txt(31, 6, "<Win>");
        else txt(31, 7, "<Alt>");

        /* green */
        uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_FONT;
        uint32_t values[] = { get_colorpixel(conn, "#00FF00"), font_id };
        xcb_change_gc(conn, pixmap_gc, mask, values);

        txt(25, 11, "<Enter>");

        /* red */
        xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#FF0000"));
        txt(31, 12, "<ESC>");
    }

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, /* */ 500, 500);
    xcb_flush(conn);

    return 1;
}

static int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    printf("Keypress %d, state raw = %d\n", event->detail, event->state);

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, event->state);

    printf("sym = %c (%d)\n", sym, sym);

    if (sym == XK_Return) {
        if (current_step == STEP_WELCOME)
            current_step = STEP_GENERATE;
        else finish();
    }

    /* cancel any time */
    if (sym == XK_Escape)
        exit(0);

    if (sym == XK_Alt_L)
        modifier = MOD_ALT;

    if (sym == XK_Super_L)
        modifier = MOD_SUPER;

    handle_expose();
    return 1;
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
    ssize_t read;
    bool head_of_file = true;

    /* write a header about auto-generation to the output file */
    fputs("# This file has been auto-generated by i3-config-wizard(1).\n", ks_config);
    fputs("# It will not be overwritten, so edit it as you like.\n", ks_config);
    fputs("#\n", ks_config);
    fputs("# Should you change your keyboard layout somewhen, delete\n", ks_config);
    fputs("# this file and re-run i3-config-wizard(1).\n", ks_config);
    fputs("#\n", ks_config);

    while ((read = getline(&line, &len, kc_config)) != -1) {
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
            if (modifier == MOD_ALT)
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
    int sockfd = connect_ipc(socket_path);
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
        socket_path = socket_path_from_x11();

    if (socket_path == NULL)
        socket_path = "/tmp/i3-ipc.sock";

    int screens;
    conn = xcb_connect(NULL, &screens);
    if (xcb_connection_has_error(conn))
        errx(1, "Cannot open display\n");

    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screens);
    root = root_screen->root;

    symbols = xcb_key_symbols_alloc(conn);

    font_id = get_font_id(conn, pattern, &font_height);
    font_bold_id = get_font_id(conn, patternbold, &font_bold_height);

    /* Open an input window */
    win = open_input_window(conn, 300, 205);

    /* Create pixmap */
    pixmap = xcb_generate_id(conn);
    pixmap_gc = xcb_generate_id(conn);
    xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, 500, 500);
    xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);

    /* Set input focus (we have override_redirect=1, so the wm will not do
     * this for us) */
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);

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

            case XCB_EXPOSE:
                handle_expose();
                break;
        }

        free(event);
    }

    return 0;
}
