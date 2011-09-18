/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * i3-input/main.c: Utility which lets the user input commands and sends them
 * to i3.
 *
 */
#include <ev.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <getopt.h>
#include <limits.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>

#include <X11/keysym.h>

#include "keysym2ucs.h"

#include "i3-input.h"

/* IPC format string. %s will be replaced with what the user entered, then
 * the command will be sent to i3 */
static char *format;

static char *socket_path;
static int sockfd;
static xcb_key_symbols_t *symbols;
static int modeswitchmask;
static int numlockmask;
static bool modeswitch_active = false;
static xcb_window_t win;
static xcb_pixmap_t pixmap;
static xcb_gcontext_t pixmap_gc;
static char *glyphs_ucs[512];
static char *glyphs_utf8[512];
static int input_position;
static int font_height;
static char *prompt;
static int prompt_len;
static int limit;
xcb_window_t root;

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
 * Concats the glyphs (either UCS-2 or UTF-8) to a single string, suitable for
 * rendering it (UCS-2) or sending it to i3 (UTF-8).
 *
 */
static uint8_t *concat_strings(char **glyphs, int max) {
    uint8_t *output = calloc(max+1, 4);
    uint8_t *walk = output;
    for (int c = 0; c < max; c++) {
        printf("at %c\n", glyphs[c][0]);
        /* if the first byte is 0, this has to be UCS2 */
        if (glyphs[c][0] == '\0') {
            memcpy(walk, glyphs[c], 2);
            walk += 2;
        } else {
            strcpy((char*)walk, glyphs[c]);
            walk += strlen(glyphs[c]);
        }
    }
    printf("output = %s\n", output);
    return output;
}

/*
 * Handles expose events (redraws of the window) and rendering in general. Will
 * be called from the code with event == NULL or from X with event != NULL.
 *
 */
static int handle_expose(void *data, xcb_connection_t *conn, xcb_expose_event_t *event) {
    printf("expose!\n");

    /* re-draw the background */
    xcb_rectangle_t border = {0, 0, 500, font_height + 8}, inner = {2, 2, 496, font_height + 8 - 4};
    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#FF0000"));
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);
    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &inner);

    /* restore font color */
    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#FFFFFF"));
    uint8_t *con = concat_strings(glyphs_ucs, input_position);
    char *full_text = (char*)con;
    if (prompt != NULL) {
        full_text = malloc((prompt_len + input_position) * 2 + 1);
        if (full_text == NULL)
            err(EXIT_FAILURE, "malloc() failed\n");
        memcpy(full_text, prompt, prompt_len * 2);
        memcpy(full_text + (prompt_len * 2), con, input_position * 2);
    }
    xcb_image_text_16(conn, input_position + prompt_len, pixmap, pixmap_gc, 4 /* X */,
                      font_height + 2 /* Y = baseline of font */, (xcb_char2b_t*)full_text);

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, /* */ 500, font_height + 8);
    xcb_flush(conn);
    free(con);
    if (prompt != NULL)
        free(full_text);

    return 1;
}

/*
 * Deactivates the Mode_switch bit upon release of the Mode_switch key.
 *
 */
static int handle_key_release(void *ignored, xcb_connection_t *conn, xcb_key_release_event_t *event) {
    printf("releasing %d, state raw = %d\n", event->detail, event->state);

    /* fix state */
    event->state &= ~numlockmask;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, event->state);
    if (sym == XK_Mode_switch) {
        printf("Mode switch disabled\n");
        modeswitch_active = false;
    }

    return 1;
}

static void finish_input() {
    char *command = (char*)concat_strings(glyphs_utf8, input_position);

    /* count the occurences of %s in the string */
    int c;
    int len = strlen(format);
    int cnt = 0;
    for (c = 0; c < (len-1); c++)
        if (format[c] == '%' && format[c+1] == 's')
            cnt++;
    printf("occurences = %d\n", cnt);

    /* allocate space for the output */
    int inputlen = strlen(command);
    char *full = calloc(1,
                        strlen(format) - (2 * cnt) /* format without all %s */
                        + (inputlen * cnt)         /* replaced %s */
                        + 1);                      /* trailing NUL */
    char *dest = full;
    for (c = 0; c < len; c++) {
        /* if this is not % or it is % but without a following 's',
         * just copy the character */
        if (format[c] != '%' || (c == (len-1)) || format[c+1] != 's')
            *(dest++) = format[c];
        else {
            strncat(dest, command, inputlen);
            dest += inputlen;
            /* skip the following 's' of '%s' */
            c++;
        }
    }

    /* prefix the command if a prefix was specified on commandline */
    printf("command = %s\n", full);

    ipc_send_message(sockfd, strlen(full), 0, (uint8_t*)full);

#if 0
    free(command);
    return 1;
#endif
    exit(0);
}

/*
 * Handles keypresses by converting the keycodes to keysymbols, then the
 * keysymbols to UCS-2. If the conversion succeeded, the glyph is saved in the
 * internal buffers and displayed in the input window.
 *
 * Also handles backspace (deleting one character) and return (sending the
 * command to i3).
 *
 */
static int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    printf("Keypress %d, state raw = %d\n", event->detail, event->state);

    /* fix state */
    if (modeswitch_active)
        event->state |= modeswitchmask;

    /* Apparantly, after activating numlock once, the numlock modifier
     * stays turned on (use xev(1) to verify). So, to resolve useful
     * keysyms, we remove the numlock flag from the event state */
    event->state &= ~numlockmask;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, event->state);
    if (sym == XK_Mode_switch) {
        printf("Mode switch enabled\n");
        modeswitch_active = true;
        return 1;
    }

    if (sym == XK_Return)
        finish_input();

    if (sym == XK_BackSpace) {
        if (input_position == 0)
            return 1;

        input_position--;
        free(glyphs_ucs[input_position]);
        free(glyphs_utf8[input_position]);

        handle_expose(NULL, conn, NULL);
        return 1;
    }
    if (sym == XK_Escape) {
        exit(0);
    }

    /* TODO: handle all of these? */
    printf("is_keypad_key = %d\n", xcb_is_keypad_key(sym));
    printf("is_private_keypad_key = %d\n", xcb_is_private_keypad_key(sym));
    printf("xcb_is_cursor_key = %d\n", xcb_is_cursor_key(sym));
    printf("xcb_is_pf_key = %d\n", xcb_is_pf_key(sym));
    printf("xcb_is_function_key = %d\n", xcb_is_function_key(sym));
    printf("xcb_is_misc_function_key = %d\n", xcb_is_misc_function_key(sym));
    printf("xcb_is_modifier_key = %d\n", xcb_is_modifier_key(sym));

    if (xcb_is_modifier_key(sym) || xcb_is_cursor_key(sym))
        return 1;

    printf("sym = %c (%d)\n", sym, sym);

    /* convert the keysym to UCS */
    uint16_t ucs = keysym2ucs(sym);
    if ((int16_t)ucs == -1) {
        fprintf(stderr, "Keysym could not be converted to UCS, skipping\n");
        return 1;
    }

    /* store the UCS into a string */
    uint8_t inp[3] = {(ucs & 0xFF00) >> 8, (ucs & 0xFF), 0};

    printf("inp[0] = %02x, inp[1] = %02x, inp[2] = %02x\n", inp[0], inp[1], inp[2]);
    /* convert it to UTF-8 */
    char *out = convert_ucs_to_utf8((char*)inp);
    printf("converted to %s\n", out);

    glyphs_ucs[input_position] = malloc(3 * sizeof(uint8_t));
    if (glyphs_ucs[input_position] == NULL)
        err(EXIT_FAILURE, "malloc() failed\n");
    memcpy(glyphs_ucs[input_position], inp, 3);
    glyphs_utf8[input_position] = strdup(out);
    input_position++;

    if (input_position == limit)
        finish_input();

    handle_expose(NULL, conn, NULL);
    return 1;
}

int main(int argc, char *argv[]) {
    format = strdup("%s");
    socket_path = getenv("I3SOCK");
    char *pattern = "-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";
    int o, option_index = 0;

    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {"limit", required_argument, 0, 'l'},
        {"prompt", required_argument, 0, 'P'},
        {"prefix", required_argument, 0, 'p'},
        {"format", required_argument, 0, 'F'},
        {"font", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    char *options_string = "s:p:P:f:l:F:vh";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        switch (o) {
            case 's':
                FREE(socket_path);
                socket_path = strdup(optarg);
                break;
            case 'v':
                printf("i3-input " I3_VERSION);
                return 0;
            case 'p':
                /* This option is deprecated, but will still work in i3 v4.1, 4.2 and 4.3 */
                fprintf(stderr, "i3-input: WARNING: the -p option is DEPRECATED in favor of the -F (format) option\n");
                FREE(format);
                asprintf(&format, "%s%%s", optarg);
                break;
            case 'l':
                limit = atoi(optarg);
                break;
            case 'P':
                FREE(prompt);
                prompt = strdup(optarg);
                break;
            case 'f':
                FREE(pattern);
                pattern = strdup(optarg);
                break;
            case 'F':
                FREE(format);
                format = strdup(optarg);
                break;
            case 'h':
                printf("i3-input " I3_VERSION "\n");
                printf("i3-input [-s <socket>] [-F <format>] [-l <limit>] [-P <prompt>] [-f <font>] [-v]\n");
                printf("\n");
                printf("Example:\n");
                printf("    i3-input -F 'workspace \"%%s\"' -P 'Switch to workspace: '\n");
                return 0;
        }
    }

    printf("using format \"%s\"\n", format);

    if (socket_path == NULL)
        socket_path = socket_path_from_x11();

    if (socket_path == NULL)
        socket_path = "/tmp/i3-ipc.sock";

    sockfd = connect_ipc(socket_path);

    if (prompt != NULL)
        prompt = convert_utf8_to_ucs2(prompt, &prompt_len);

    int screens;
    xcb_connection_t *conn = xcb_connect(NULL, &screens);
    if (xcb_connection_has_error(conn))
        die("Cannot open display\n");

    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screens);
    root = root_screen->root;

    modeswitchmask = get_mod_mask(conn, XK_Mode_switch);
    numlockmask = get_mod_mask(conn, XK_Num_Lock);
    symbols = xcb_key_symbols_alloc(conn);

    uint32_t font_id = get_font_id(conn, pattern, &font_height);

    /* Open an input window */
    win = open_input_window(conn, 500, font_height + 8);

    /* Create pixmap */
    pixmap = xcb_generate_id(conn);
    pixmap_gc = xcb_generate_id(conn);
    xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, 500, font_height + 8);
    xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);

    /* Set input focus (we have override_redirect=1, so the wm will not do
     * this for us) */
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);

    /* Create graphics context */
    xcb_change_gc_single(conn, pixmap_gc, XCB_GC_FONT, font_id);

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

            case XCB_KEY_RELEASE:
                handle_key_release(NULL, conn, (xcb_key_release_event_t*)event);
                break;

            case XCB_EXPOSE:
                handle_expose(NULL, conn, (xcb_expose_event_t*)event);
                break;
        }

        free(event);
    }

    return 0;
}
