/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2013 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-nagbar is a utility which displays a nag message, for example in the case
 * when the user has an error in his configuration file.
 *
 */
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

#include "libi3.h"
#include "i3-nagbar.h"

static char *argv0 = NULL;

typedef struct {
    i3String *label;
    char *action;
    int16_t x;
    uint16_t width;
} button_t;

static xcb_window_t win;
static xcb_pixmap_t pixmap;
static xcb_gcontext_t pixmap_gc;
static xcb_rectangle_t rect = {0, 0, 600, 20};
static i3Font font;
static i3String *prompt;
static button_t *buttons;
static int buttoncnt;

/* Result of get_colorpixel() for the various colors. */
static uint32_t color_background;        /* background of the bar */
static uint32_t color_button_background; /* background for buttons */
static uint32_t color_border;            /* color of the button border */
static uint32_t color_border_bottom;     /* color of the bottom border */
static uint32_t color_text;              /* color of the text */

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
    if (event->event_x >= (rect.width - 32))
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
    symlink(exe_path, link_path);

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
 * Handles expose events (redraws of the window) and rendering in general. Will
 * be called from the code with event == NULL or from X with event != NULL.
 *
 */
static int handle_expose(xcb_connection_t *conn, xcb_expose_event_t *event) {
    /* re-draw the background */
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]) {color_background});
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &rect);

    /* restore font color */
    set_font_colors(pixmap_gc, color_text, color_background);
    draw_text(prompt, pixmap, pixmap_gc,
              4 + 4, 4 + 4, rect.width - 4 - 4);

    /* render close button */
    const char *close_button_label = "X";
    int line_width = 4;
    /* set width to the width of the label */
    int w = predict_text_width(i3string_from_utf8(close_button_label));
    /* account for left/right padding, which seems to be set to 8px (total) below */
    w += 8;
    int y = rect.width;
    uint32_t values[3];
    values[0] = color_button_background;
    values[1] = line_width;
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH, values);

    xcb_rectangle_t close = {y - w - (2 * line_width), 0, w + (2 * line_width), rect.height};
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &close);

    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]) {color_border});
    xcb_point_t points[] = {
        {y - w - (2 * line_width), line_width / 2},
        {y - (line_width / 2), line_width / 2},
        {y - (line_width / 2), (rect.height - (line_width / 2)) - 2},
        {y - w - (2 * line_width), (rect.height - (line_width / 2)) - 2},
        {y - w - (2 * line_width), line_width / 2}};
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, pixmap, pixmap_gc, 5, points);

    values[0] = 1;
    set_font_colors(pixmap_gc, color_text, color_button_background);
    /* the x term here seems to set left/right padding */
    draw_text_ascii(close_button_label, pixmap, pixmap_gc, y - w - line_width + w / 2 - 4,
                    4 + 4 - 1, rect.width - y + w + line_width - w / 2 + 4);
    y -= w;

    y -= 20;

    /* render custom buttons */
    line_width = 1;
    for (int c = 0; c < buttoncnt; c++) {
        /* set w to the width of the label */
        w = predict_text_width(buttons[c].label);
        /* account for left/right padding, which seems to be set to 12px (total) below */
        w += 12;
        y -= 30;
        xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]) {color_button_background});
        close = (xcb_rectangle_t) {y - w - (2 * line_width), 2, w + (2 * line_width), rect.height - 6};
        xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &close);

        xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]) {color_border});
        buttons[c].x = y - w - (2 * line_width);
        buttons[c].width = w;
        xcb_point_t points2[] = {
            {y - w - (2 * line_width), (line_width / 2) + 2},
            {y - (line_width / 2), (line_width / 2) + 2},
            {y - (line_width / 2), (rect.height - 4 - (line_width / 2))},
            {y - w - (2 * line_width), (rect.height - 4 - (line_width / 2))},
            {y - w - (2 * line_width), (line_width / 2) + 2}};
        xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, pixmap, pixmap_gc, 5, points2);

        values[0] = color_text;
        values[1] = color_button_background;
        set_font_colors(pixmap_gc, color_text, color_button_background);
        /* the x term seems to set left/right padding */
        draw_text(buttons[c].label, pixmap, pixmap_gc,
                  y - w - line_width + 6, 4 + 3, rect.width - y + w + line_width - 6);

        y -= w;
    }

    /* border line at the bottom */
    line_width = 2;
    values[0] = color_border_bottom;
    values[1] = line_width;
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH, values);
    xcb_point_t bottom[] = {
        {0, rect.height - 0},
        {rect.width, rect.height - 0}};
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, pixmap, pixmap_gc, 2, bottom);

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, rect.width, rect.height);
    xcb_flush(conn);

    return 1;
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
        cmd = strdup(argv[0]);
        *(cmd + argv0_len - strlen(".nagbar_cmd")) = '\0';
        execl("/bin/sh", "/bin/sh", cmd, NULL);
        err(EXIT_FAILURE, "execv(/bin/sh, /bin/sh, %s)", cmd);
    }

    argv0 = argv[0];

    char *pattern = sstrdup("-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1");
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
                buttons = realloc(buttons, sizeof(button_t) * (buttoncnt + 1));
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
        color_button_background = get_colorpixel("#680a0a");
        color_background = get_colorpixel("#900000");
        color_text = get_colorpixel("#ffffff");
        color_border = get_colorpixel("#d92424");
        color_border_bottom = get_colorpixel("#470909");
    } else {
        /* Yellowish theme for warnings */
        color_button_background = get_colorpixel("#ffc100");
        color_background = get_colorpixel("#ffa8000");
        color_text = get_colorpixel("#000000");
        color_border = get_colorpixel("#ab7100");
        color_border_bottom = get_colorpixel("#ab7100");
    }

    font = load_font(pattern, true);
    set_font(&font);

    /* Open an input window */
    win = xcb_generate_id(conn);

    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        win,                                                 /* the window id */
        root,                                                /* parent == root */
        50, 50, 500, font.height + 8 + 8 /* 8 px padding */, /* dimensions */
        0,                                                   /* x11 border = 0, we draw our own */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        (uint32_t[]) {
            0, /* back pixel: black */
            XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE});

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

    strut_partial.top = font.height + 6;
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

    /* Create pixmap */
    pixmap = xcb_generate_id(conn);
    pixmap_gc = xcb_generate_id(conn);
    xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, 500, font.height + 8);
    xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);

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
                handle_expose(conn, (xcb_expose_event_t *)event);
                break;

            case XCB_BUTTON_PRESS:
                handle_button_press(conn, (xcb_button_press_event_t *)event);
                break;

            case XCB_BUTTON_RELEASE:
                handle_button_release(conn, (xcb_button_release_event_t *)event);
                break;

            case XCB_CONFIGURE_NOTIFY: {
                xcb_configure_notify_event_t *configure_notify = (xcb_configure_notify_event_t *)event;
                rect = (xcb_rectangle_t) {
                    configure_notify->x,
                    configure_notify->y,
                    configure_notify->width,
                    configure_notify->height};

                /* Recreate the pixmap / gc */
                xcb_free_pixmap(conn, pixmap);
                xcb_free_gc(conn, pixmap_gc);

                xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, rect.width, rect.height);
                xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);
                break;
            }
        }

        free(event);
    }

    FREE(pattern);

    return 0;
}
