/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-nagbar is a utility which displays a nag message, for example in the case
 * when the user has an error in their configuration file.
 *
 */
#include "libi3.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <paths.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/randr.h>
#include <xcb/xcb_cursor.h>

#include "i3-nagbar.h"

/** This is the equivalent of XC_left_ptr. I’m not sure why xcb doesn’t have a
 * constant for that. */
#define XCB_CURSOR_LEFT_PTR 68

#define MSG_PADDING logical_px(8)
#define BTN_PADDING logical_px(3)
#define BTN_BORDER logical_px(3)
#define BTN_GAP logical_px(20)
#define CLOSE_BTN_GAP logical_px(15)
#define BAR_BORDER logical_px(2)

static char *argv0 = NULL;

typedef struct {
    i3String *label;
    char *action;
    int16_t x;
    uint16_t width;
} button_t;

static xcb_window_t win;
static surface_t bar;

static i3Font font;
static i3String *prompt;

static button_t btn_close;
static button_t *buttons;
static int buttoncnt;

/* Result of get_colorpixel() for the various colors. */
static color_t color_background;        /* background of the bar */
static color_t color_button_background; /* background for buttons */
static color_t color_border;            /* color of the button border */
static color_t color_border_bottom;     /* color of the bottom border */
static color_t color_text;              /* color of the text */

xcb_window_t root;
xcb_connection_t *conn;
xcb_screen_t *root_screen;

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
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If it
 * does not exist, /bin/sh is used.
 *
 */
static void start_application(const char *command) {
    printf("executing: %s\n", command);
    if (fork() == 0) {
        /* Child process */
        setsid();
        if (fork() == 0) {
            /* This is the child */
            execl(_PATH_BSHELL, _PATH_BSHELL, "-c", command, (void *)NULL);
            /* not reached */
        }
        exit(0);
    }
    wait(0);
}

static button_t *get_button_at(int16_t x, int16_t y) {
    for (int c = 0; c < buttoncnt; c++)
        if (x >= (buttons[c].x) && x <= (buttons[c].x + buttons[c].width))
            return &buttons[c];

    return NULL;
}

static void handle_button_press(xcb_connection_t *conn, xcb_button_press_event_t *event) {
    printf("button pressed on x = %d, y = %d\n",
           event->event_x, event->event_y);
    /* TODO: set a flag for the button, re-render */
}

/*
 * Called when the user releases the mouse button. Checks whether the
 * coordinates are over a button and executes the appropriate action.
 *
 */
static void handle_button_release(xcb_connection_t *conn, xcb_button_release_event_t *event) {
    printf("button released on x = %d, y = %d\n",
           event->event_x, event->event_y);
    /* If the user hits the close button, we exit(0) */
    if (event->event_x >= btn_close.x && event->event_x < btn_close.x + btn_close.width)
        exit(0);
    button_t *button = get_button_at(event->event_x, event->event_y);
    if (!button)
        return;

    /* We need to create a custom script containing our actual command
     * since not every terminal emulator which is contained in
     * i3-sensible-terminal supports -e with multiple arguments (and not
     * all of them support -e with one quoted argument either).
     *
     * NB: The paths need to be unique, that is, don’t assume users close
     * their nagbars at any point in time (and they still need to work).
     * */
    char *script_path = get_process_filename("nagbar-cmd");

    int fd = open(script_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        warn("Could not create temporary script to store the nagbar command");
        return;
    }
    FILE *script = fdopen(fd, "w");
    if (script == NULL) {
        warn("Could not fdopen() temporary script to store the nagbar command");
        return;
    }
    fprintf(script, "#!/bin/sh\nrm %s\n%s", script_path, button->action);
    /* Also closes fd */
    fclose(script);

    char *link_path;
    char *exe_path = get_exe_path(argv0);
    sasprintf(&link_path, "%s.nagbar_cmd", script_path);
    if (symlink(exe_path, link_path) == -1) {
        err(EXIT_FAILURE, "Failed to symlink %s to %s", link_path, exe_path);
    }

    char *terminal_cmd;
    sasprintf(&terminal_cmd, "i3-sensible-terminal -e %s", link_path);
    printf("argv0 = %s\n", argv0);
    printf("terminal_cmd = %s\n", terminal_cmd);

    start_application(terminal_cmd);

    free(link_path);
    free(terminal_cmd);
    free(script_path);
    free(exe_path);

    /* TODO: unset flag, re-render */
}

/*
 * Draws a button and returns its width
 *
 */
static int button_draw(button_t *button, int position) {
    int text_width = predict_text_width(button->label);
    button->width = text_width + 2 * BTN_PADDING + 2 * BTN_BORDER;
    button->x = position - button->width;

    /* draw border */
    draw_util_rectangle(&bar, color_border,
                        position - button->width,
                        MSG_PADDING - BTN_PADDING - BTN_BORDER,
                        button->width,
                        font.height + 2 * BTN_PADDING + 2 * BTN_BORDER);
    /* draw background */
    draw_util_rectangle(&bar, color_button_background,
                        position - button->width + BTN_BORDER,
                        MSG_PADDING - BTN_PADDING,
                        text_width + 2 * BTN_PADDING,
                        font.height + 2 * BTN_PADDING);
    /* draw label */
    draw_util_text(button->label, &bar, color_text, color_button_background,
                   position - button->width + BTN_BORDER + BTN_PADDING,
                   MSG_PADDING,
                   200);
    return button->width;
}

/*
 * Handles expose events (redraws of the window) and rendering in general. Will
 * be called from the code with event == NULL or from X with event != NULL.
 *
 */
static int handle_expose(xcb_connection_t *conn, xcb_expose_event_t *event) {
    /* draw background */
    draw_util_clear_surface(&bar, color_background);
    /* draw message */
    draw_util_text(prompt, &bar, color_text, color_background,
                   MSG_PADDING, MSG_PADDING,
                   bar.width - 2 * MSG_PADDING);

    int position = bar.width - (MSG_PADDING - BTN_BORDER - BTN_PADDING);

    /* render close button */
    position -= button_draw(&btn_close, position);
    position -= CLOSE_BTN_GAP;

    /* render custom buttons */
    for (int i = 0; i < buttoncnt; i++) {
        position -= BTN_GAP;
        position -= button_draw(&buttons[i], position);
    }

    /* border line at the bottom */
    draw_util_rectangle(&bar, color_border_bottom, 0, bar.height - BAR_BORDER, bar.width, BAR_BORDER);

    xcb_flush(conn);
    return 1;
}

/**
 * Return the position and size the i3-nagbar window should use.
 * This will be the primary output or a fallback if it cannot be determined.
 */
static xcb_rectangle_t get_window_position(void) {
    /* Default values if we cannot determine the primary output or its CRTC info. */
    xcb_rectangle_t result = (xcb_rectangle_t){50, 50, 500, font.height + 2 * MSG_PADDING + BAR_BORDER};

    xcb_randr_get_screen_resources_current_cookie_t rcookie = xcb_randr_get_screen_resources_current(conn, root);
    xcb_randr_get_output_primary_cookie_t pcookie = xcb_randr_get_output_primary(conn, root);

    xcb_randr_get_output_primary_reply_t *primary = NULL;
    xcb_randr_get_screen_resources_current_reply_t *res = NULL;

    if ((primary = xcb_randr_get_output_primary_reply(conn, pcookie, NULL)) == NULL) {
        DLOG("Could not determine the primary output.\n");
        goto free_resources;
    }

    if ((res = xcb_randr_get_screen_resources_current_reply(conn, rcookie, NULL)) == NULL) {
        goto free_resources;
    }

    xcb_randr_get_output_info_reply_t *output =
        xcb_randr_get_output_info_reply(conn,
                                        xcb_randr_get_output_info(conn, primary->output, res->config_timestamp),
                                        NULL);
    if (output == NULL || output->crtc == XCB_NONE)
        goto free_resources;

    xcb_randr_get_crtc_info_reply_t *crtc =
        xcb_randr_get_crtc_info_reply(conn,
                                      xcb_randr_get_crtc_info(conn, output->crtc, res->config_timestamp),
                                      NULL);
    if (crtc == NULL)
        goto free_resources;

    DLOG("Found primary output on position x = %i / y = %i / w = %i / h = %i.\n",
         crtc->x, crtc->y, crtc->width, crtc->height);
    if (crtc->width == 0 || crtc->height == 0) {
        DLOG("Primary output is not active, ignoring it.\n");
        goto free_resources;
    }

    result.x = crtc->x;
    result.y = crtc->y;
    goto free_resources;

free_resources:
    FREE(res);
    FREE(primary);
    return result;
}

int main(int argc, char *argv[]) {
    /* The following lines are a terribly horrible kludge. Because terminal
     * emulators have different ways of interpreting the -e command line
     * argument (some need -e "less /etc/fstab", others need -e less
     * /etc/fstab), we need to write commands to a script and then just run
     * that script. However, since on some machines, $XDG_RUNTIME_DIR and
     * $TMPDIR are mounted with noexec, we cannot directly execute the script
     * either.
     *
     * Initially, we tried to pass the command via the environment variable
     * _I3_NAGBAR_CMD. But turns out that some terminal emulators such as
     * xfce4-terminal run all windows from a single master process and only
     * pass on the command (not the environment) to that master process.
     *
     * Therefore, we symlink i3-nagbar (which MUST reside on an executable
     * filesystem) with a special name and run that symlink. When i3-nagbar
     * recognizes it’s started as a binary ending in .nagbar_cmd, it strips off
     * the .nagbar_cmd suffix and runs /bin/sh on argv[0]. That way, we can run
     * a shell script on a noexec filesystem.
     *
     * From a security point of view, i3-nagbar is just an alias to /bin/sh in
     * certain circumstances. This should not open any new security issues, I
     * hope. */
    char *cmd = NULL;
    const size_t argv0_len = strlen(argv[0]);
    if (argv0_len > strlen(".nagbar_cmd") &&
        strcmp(argv[0] + argv0_len - strlen(".nagbar_cmd"), ".nagbar_cmd") == 0) {
        unlink(argv[0]);
        cmd = sstrdup(argv[0]);
        *(cmd + argv0_len - strlen(".nagbar_cmd")) = '\0';
        execl("/bin/sh", "/bin/sh", cmd, NULL);
        err(EXIT_FAILURE, "execv(/bin/sh, /bin/sh, %s)", cmd);
    }

    argv0 = argv[0];

    char *pattern = sstrdup("pango:monospace 8");
    int o, option_index = 0;
    enum { TYPE_ERROR = 0,
           TYPE_WARNING = 1 } bar_type = TYPE_ERROR;

    static struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"font", required_argument, 0, 'f'},
        {"button", required_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {"message", required_argument, 0, 'm'},
        {"type", required_argument, 0, 't'},
        {0, 0, 0, 0}};

    char *options_string = "b:f:m:t:vh";

    prompt = i3string_from_utf8("Please do not run this program.");

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        switch (o) {
            case 'v':
                printf("i3-nagbar " I3_VERSION "\n");
                return 0;
            case 'f':
                FREE(pattern);
                pattern = sstrdup(optarg);
                break;
            case 'm':
                i3string_free(prompt);
                prompt = i3string_from_utf8(optarg);
                break;
            case 't':
                bar_type = (strcasecmp(optarg, "warning") == 0 ? TYPE_WARNING : TYPE_ERROR);
                break;
            case 'h':
                printf("i3-nagbar " I3_VERSION "\n");
                printf("i3-nagbar [-m <message>] [-b <button> <action>] [-t warning|error] [-f <font>] [-v]\n");
                return 0;
            case 'b':
                buttons = srealloc(buttons, sizeof(button_t) * (buttoncnt + 1));
                buttons[buttoncnt].label = i3string_from_utf8(optarg);
                buttons[buttoncnt].action = argv[optind];
                printf("button with label *%s* and action *%s*\n",
                       i3string_as_utf8(buttons[buttoncnt].label),
                       buttons[buttoncnt].action);
                buttoncnt++;
                printf("now %d buttons\n", buttoncnt);
                if (optind < argc)
                    optind++;
                break;
        }
    }

    btn_close.label = i3string_from_utf8("X");

    int screens;
    if ((conn = xcb_connect(NULL, &screens)) == NULL ||
        xcb_connection_has_error(conn))
        die("Cannot open display\n");

/* Place requests for the atoms we need as soon as possible */
#define xmacro(atom) \
    xcb_intern_atom_cookie_t atom##_cookie = xcb_intern_atom(conn, 0, strlen(#atom), #atom);
#include "atoms.xmacro"
#undef xmacro

    root_screen = xcb_aux_get_screen(conn, screens);
    root = root_screen->root;

    if (bar_type == TYPE_ERROR) {
        /* Red theme for error messages */
        color_button_background = draw_util_hex_to_color("#680a0a");
        color_background = draw_util_hex_to_color("#900000");
        color_text = draw_util_hex_to_color("#ffffff");
        color_border = draw_util_hex_to_color("#d92424");
        color_border_bottom = draw_util_hex_to_color("#470909");
    } else {
        /* Yellowish theme for warnings */
        color_button_background = draw_util_hex_to_color("#ffc100");
        color_background = draw_util_hex_to_color("#ffa8000");
        color_text = draw_util_hex_to_color("#000000");
        color_border = draw_util_hex_to_color("#ab7100");
        color_border_bottom = draw_util_hex_to_color("#ab7100");
    }

    init_dpi();
    font = load_font(pattern, true);
    set_font(&font);

#if defined(__OpenBSD__)
    if (pledge("stdio rpath wpath cpath getpw proc exec", NULL) == -1)
        err(EXIT_FAILURE, "pledge");
#endif

    xcb_rectangle_t win_pos = get_window_position();

    xcb_cursor_t cursor;
    xcb_cursor_context_t *cursor_ctx;
    if (xcb_cursor_context_new(conn, root_screen, &cursor_ctx) == 0) {
        cursor = xcb_cursor_load_cursor(cursor_ctx, "left_ptr");
        xcb_cursor_context_free(cursor_ctx);
    } else {
        cursor = xcb_generate_id(conn);
        i3Font cursor_font = load_font("cursor", false);
        xcb_create_glyph_cursor(
            conn,
            cursor,
            cursor_font.specific.xcb.id,
            cursor_font.specific.xcb.id,
            XCB_CURSOR_LEFT_PTR,
            XCB_CURSOR_LEFT_PTR + 1,
            0, 0, 0,
            65535, 65535, 65535);
    }

    /* Open an input window */
    win = xcb_generate_id(conn);

    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        win,                                                 /* the window id */
        root,                                                /* parent == root */
        win_pos.x, win_pos.y, win_pos.width, win_pos.height, /* dimensions */
        0,                                                   /* x11 border = 0, we draw our own */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_CURSOR,
        (uint32_t[]){
            0, /* back pixel: black */
            XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE,
            cursor});

    /* Map the window (make it visible) */
    xcb_map_window(conn, win);

/* Setup NetWM atoms */
#define xmacro(name)                                                                       \
    do {                                                                                   \
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, name##_cookie, NULL); \
        if (!reply)                                                                        \
            die("Could not get atom " #name "\n");                                         \
                                                                                           \
        A_##name = reply->atom;                                                            \
        free(reply);                                                                       \
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
                        (unsigned char *)&A__NET_WM_WINDOW_TYPE_DOCK);

    /* Reserve some space at the top of the screen */
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
    } __attribute__((__packed__)) strut_partial;
    memset(&strut_partial, 0, sizeof(strut_partial));

    strut_partial.top = font.height + logical_px(6);
    strut_partial.top_start_x = 0;
    strut_partial.top_end_x = 800;

    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        A__NET_WM_STRUT_PARTIAL,
                        A_CARDINAL,
                        32,
                        12,
                        &strut_partial);

    /* Initialize the drawable bar */
    draw_util_surface_init(conn, &bar, win, get_visualtype(root_screen), win_pos.width, win_pos.height);

    /* Grab the keyboard to get all input */
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
            case XCB_EXPOSE:
                if (((xcb_expose_event_t *)event)->count == 0) {
                    handle_expose(conn, (xcb_expose_event_t *)event);
                }

                break;

            case XCB_BUTTON_PRESS:
                handle_button_press(conn, (xcb_button_press_event_t *)event);
                break;

            case XCB_BUTTON_RELEASE:
                handle_button_release(conn, (xcb_button_release_event_t *)event);
                break;

            case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t *configure_notify = (xcb_configure_notify_event_t *)event;
                if (configure_notify->width > 0 && configure_notify->height > 0) {
                    draw_util_surface_set_size(&bar, configure_notify->width, configure_notify->height);
                }
                break;
            }
        }

        free(event);
    }

    FREE(pattern);
    draw_util_surface_free(conn, &bar);

    return 0;
}
