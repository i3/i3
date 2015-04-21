/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-input/main.c: Utility which lets the user input commands and sends them
 *                  to i3.
 *
 */
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

#include "libi3.h"

/* IPC format string. %s will be replaced with what the user entered, then
 * the command will be sent to i3 */
static char *format;

static char *socket_path;
static int sockfd;
static xcb_key_symbols_t *symbols;
static bool modeswitch_active = false;
static xcb_window_t win;
static xcb_pixmap_t pixmap;
static xcb_gcontext_t pixmap_gc;
static xcb_char2b_t glyphs_ucs[512];
static char *glyphs_utf8[512];
static int input_position;
static i3Font font;
static i3String *prompt;
static int prompt_offset = 0;
static int limit;
xcb_window_t root;
xcb_connection_t *conn;
xcb_screen_t *root_screen;
static xcb_get_input_focus_cookie_t focus_cookie;

/*
 * Having verboselog(), errorlog() and debuglog() is necessary when using libi3.
 *
 */
void verboselog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

void errorlog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void debuglog(char *fmt, ...) {
}

/*
 * Restores the X11 input focus to whereever it was before.
 * This is necessary because i3-input’s window has override_redirect=1
 * (→ unmanaged by the window manager) and thus i3-input changes focus itself.
 * This function is called on exit().
 *
 */
static void restore_input_focus(void) {
    xcb_generic_error_t *error;
    xcb_get_input_focus_reply_t *reply = xcb_get_input_focus_reply(conn, focus_cookie, &error);
    if (error != NULL) {
        fprintf(stderr, "[i3-input] ERROR: Could not restore input focus (X error %d)\n", error->error_code);
        return;
    }
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, reply->focus, XCB_CURRENT_TIME);
    xcb_flush(conn);
}

/*
 * Concats the glyphs (either UCS-2 or UTF-8) to a single string, suitable for
 * rendering it (UCS-2) or sending it to i3 (UTF-8).
 *
 */
static uint8_t *concat_strings(char **glyphs, int max) {
    uint8_t *output = calloc(max + 1, 4);
    uint8_t *walk = output;
    for (int c = 0; c < max; c++) {
        printf("at %c\n", glyphs[c][0]);
        /* if the first byte is 0, this has to be UCS2 */
        if (glyphs[c][0] == '\0') {
            memcpy(walk, glyphs[c], 2);
            walk += 2;
        } else {
            strcpy((char *)walk, glyphs[c]);
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
    xcb_rectangle_t border = {0, 0, logical_px(500), font.height + logical_px(8)},
                    inner = {logical_px(2), logical_px(2), logical_px(496), font.height + logical_px(8) - logical_px(4)};
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){get_colorpixel("#FF0000")});
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){get_colorpixel("#000000")});
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &inner);

    /* restore font color */
    set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));

    /* draw the prompt … */
    if (prompt != NULL) {
        draw_text(prompt, pixmap, pixmap_gc, logical_px(4), logical_px(4), logical_px(492));
    }
    /* … and the text */
    if (input_position > 0) {
        i3String *input = i3string_from_ucs2(glyphs_ucs, input_position);
        draw_text(input, pixmap, pixmap_gc, prompt_offset + logical_px(4), logical_px(4), logical_px(492));
        i3string_free(input);
    }

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, logical_px(500), font.height + logical_px(8));
    xcb_flush(conn);

    return 1;
}

/*
 * Deactivates the Mode_switch bit upon release of the Mode_switch key.
 *
 */
static int handle_key_release(void *ignored, xcb_connection_t *conn, xcb_key_release_event_t *event) {
    printf("releasing %d, state raw = %d\n", event->detail, event->state);

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, event->state);
    if (sym == XK_Mode_switch) {
        printf("Mode switch disabled\n");
        modeswitch_active = false;
    }

    return 1;
}

static void finish_input() {
    char *command = (char *)concat_strings(glyphs_utf8, input_position);

    /* count the occurences of %s in the string */
    int c;
    int len = strlen(format);
    int cnt = 0;
    for (c = 0; c < (len - 1); c++)
        if (format[c] == '%' && format[c + 1] == 's')
            cnt++;
    printf("occurences = %d\n", cnt);

    /* allocate space for the output */
    int inputlen = strlen(command);
    char *full = calloc(1,
                        strlen(format) - (2 * cnt) /* format without all %s */
                            + (inputlen * cnt)     /* replaced %s */
                            + 1);                  /* trailing NUL */
    char *dest = full;
    for (c = 0; c < len; c++) {
        /* if this is not % or it is % but without a following 's',
         * just copy the character */
        if (format[c] != '%' || (c == (len - 1)) || format[c + 1] != 's')
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

    restore_input_focus();

    xcb_aux_sync(conn);

    ipc_send_message(sockfd, strlen(full), 0, (uint8_t *)full);

    free(full);

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

    // TODO: port the input handling code from i3lock once libxkbcommon ≥ 0.5.0
    // is available in distros.

    /* See the documentation of xcb_key_symbols_get_keysym for this one.
     * Basically: We get either col 0 or col 1, depending on whether shift is
     * pressed. */
    int col = (event->state & XCB_MOD_MASK_SHIFT);

    /* If modeswitch is currently active, we need to look in group 2 or 3,
     * respectively. */
    if (modeswitch_active)
        col += 2;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, col);
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
        free(glyphs_utf8[input_position]);

        handle_expose(NULL, conn, NULL);
        return 1;
    }
    if (sym == XK_Escape) {
        restore_input_focus();
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

    xcb_char2b_t inp;
    inp.byte1 = (ucs & 0xff00) >> 2;
    inp.byte2 = (ucs & 0x00ff) >> 0;

    printf("inp.byte1 = %02x, inp.byte2 = %02x\n", inp.byte1, inp.byte2);
    /* convert it to UTF-8 */
    char *out = convert_ucs2_to_utf8(&inp, 1);
    printf("converted to %s\n", out);

    glyphs_ucs[input_position] = inp;
    glyphs_utf8[input_position] = out;
    input_position++;

    if (input_position == limit)
        finish_input();

    handle_expose(NULL, conn, NULL);
    return 1;
}

static xcb_rectangle_t get_window_position(void) {
    xcb_rectangle_t result = (xcb_rectangle_t){logical_px(50), logical_px(50), logical_px(500), font.height + logical_px(8)};

    xcb_get_input_focus_reply_t *input_focus = NULL;
    xcb_get_geometry_reply_t *geometry = NULL;
    xcb_translate_coordinates_reply_t *coordinates = NULL;

    /* In rare cases, the window holding the input focus might disappear while we are figuring out its
     * position. To avoid this, we grab the server in the meantime. */
    xcb_grab_server(conn);

    input_focus = xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), NULL);
    if (input_focus == NULL || input_focus->focus == XCB_NONE) {
        DLOG("Failed to receive the current input focus or no window has the input focus right now.\n");
        goto free_resources;
    }

    geometry = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, input_focus->focus), NULL);
    if (geometry == NULL) {
        DLOG("Failed to received window geometry.\n");
        goto free_resources;
    }

    coordinates = xcb_translate_coordinates_reply(
        conn, xcb_translate_coordinates(conn, input_focus->focus, root, geometry->x, geometry->y), NULL);
    if (coordinates == NULL) {
        DLOG("Failed to translate coordinates.\n");
        goto free_resources;
    }

    DLOG("Determined coordinates of window with input focus at x = %i / y = %i", coordinates->dst_x, coordinates->dst_y);
    result.x += coordinates->dst_x;
    result.y += coordinates->dst_y;

free_resources:
    xcb_ungrab_server(conn);
    xcb_flush(conn);

    FREE(input_focus);
    FREE(geometry);
    FREE(coordinates);
    return result;
}

int main(int argc, char *argv[]) {
    format = strdup("%s");
    socket_path = getenv("I3SOCK");
    char *pattern = sstrdup("pango:monospace 8");
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
        {0, 0, 0, 0}};

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
                sasprintf(&format, "%s%%s", optarg);
                break;
            case 'l':
                limit = atoi(optarg);
                break;
            case 'P':
                i3string_free(prompt);
                prompt = i3string_from_utf8(optarg);
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

    int screen;
    conn = xcb_connect(NULL, &screen);
    if (!conn || xcb_connection_has_error(conn))
        die("Cannot open display\n");

    if (socket_path == NULL)
        socket_path = root_atom_contents("I3_SOCKET_PATH", conn, screen);

    if (socket_path == NULL)
        socket_path = "/tmp/i3-ipc.sock";

    sockfd = ipc_connect(socket_path);

    /* Request the current InputFocus to restore when i3-input exits. */
    focus_cookie = xcb_get_input_focus(conn);

    root_screen = xcb_aux_get_screen(conn, screen);
    root = root_screen->root;

    symbols = xcb_key_symbols_alloc(conn);

    font = load_font(pattern, true);
    set_font(&font);

    if (prompt != NULL)
        prompt_offset = predict_text_width(prompt);

    const xcb_rectangle_t win_pos = get_window_position();

    /* Open an input window */
    win = xcb_generate_id(conn);
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        win,                                                 /* the window id */
        root,                                                /* parent == root */
        win_pos.x, win_pos.y, win_pos.width, win_pos.height, /* dimensions */
        0,                                                   /* X11 border = 0, we draw our own */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
        XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
        (uint32_t[]){
            0, /* back pixel: black */
            1, /* override redirect: don’t manage this window */
            XCB_EVENT_MASK_EXPOSURE});

    /* Map the window (make it visible) */
    xcb_map_window(conn, win);

    /* Create pixmap */
    pixmap = xcb_generate_id(conn);
    pixmap_gc = xcb_generate_id(conn);
    xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, logical_px(500), font.height + logical_px(8));
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
        restore_input_focus();
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
                handle_key_press(NULL, conn, (xcb_key_press_event_t *)event);
                break;

            case XCB_KEY_RELEASE:
                handle_key_release(NULL, conn, (xcb_key_release_event_t *)event);
                break;

            case XCB_EXPOSE:
                handle_expose(NULL, conn, (xcb_expose_event_t *)event);
                break;
        }

        free(event);
    }

    return 0;
}
