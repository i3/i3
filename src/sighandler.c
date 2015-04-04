#undef I3__FILE__
#define I3__FILE__ "sighandler.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 * © 2009 Jan-Erik Rediger
 *
 * sighandler.c: Interactive crash dialog upon SIGSEGV/SIGABRT/SIGFPE (offers
 *               to restart inplace).
 *
 */
#include "all.h"

#include <ev.h>
#include <iconv.h>
#include <signal.h>
#include <sys/wait.h>

#include <xcb/xcb_event.h>

#include <X11/keysym.h>

static void open_popups(void);

static xcb_gcontext_t pixmap_gc;
static xcb_pixmap_t pixmap;
static int raised_signal;

static char *crash_text[] = {
    "i3 just crashed.",
    "To debug this problem, either attach gdb now",
    "or press",
    "- 'b' to save a backtrace (needs GDB),",
    "- 'r' to restart i3 in-place or",
    "- 'f' to forget the current layout and restart"};
static int crash_text_longest = 5;
static int backtrace_string_index = 3;
static int backtrace_done = 0;

/*
 * Attach gdb to pid_parent and dump a backtrace to i3-backtrace.$pid in the
 * tmpdir
 */
static int backtrace(void) {
    char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL)
        tmpdir = "/tmp";

    pid_t pid_parent = getpid();

    char *filename = NULL;
    int suffix = 0;
    struct stat bt;
    /* Find a unique filename for the backtrace (since the PID of i3 stays the
     * same), so that we don’t overwrite earlier backtraces. */
    do {
        FREE(filename);
        sasprintf(&filename, "%s/i3-backtrace.%d.%d.txt", tmpdir, pid_parent, suffix);
        suffix++;
    } while (stat(filename, &bt) == 0);

    pid_t pid_gdb = fork();
    if (pid_gdb < 0) {
        DLOG("Failed to fork for GDB\n");
        return -1;
    } else if (pid_gdb == 0) {
        /* child */
        int stdin_pipe[2],
            stdout_pipe[2];

        if (pipe(stdin_pipe) == -1) {
            ELOG("Failed to init stdin_pipe\n");
            return -1;
        }
        if (pipe(stdout_pipe) == -1) {
            ELOG("Failed to init stdout_pipe\n");
            return -1;
        }

        /* close standard streams in case i3 is started from a terminal; gdb
         * needs to run without controlling terminal for it to work properly in
         * this situation */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        /* We provide pipe file descriptors for stdin/stdout because gdb < 7.5
         * crashes otherwise, see
         * http://sourceware.org/bugzilla/show_bug.cgi?id=14114 */
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        char *pid_s, *gdb_log_cmd;
        sasprintf(&pid_s, "%d", pid_parent);
        sasprintf(&gdb_log_cmd, "set logging file %s", filename);

        char *args[] = {
            "gdb",
            start_argv[0],
            "-p",
            pid_s,
            "-batch",
            "-nx",
            "-ex", gdb_log_cmd,
            "-ex", "set logging on",
            "-ex", "bt full",
            "-ex", "quit",
            NULL};
        execvp(args[0], args);
        DLOG("Failed to exec GDB\n");
        exit(1);
    }
    int status = 0;

    waitpid(pid_gdb, &status, 0);

    /* see if the backtrace was succesful or not */
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        DLOG("GDB did not run properly\n");
        return -1;
    } else if (stat(filename, &bt) == -1) {
        DLOG("GDB executed successfully, but no backtrace was generated\n");
        return -1;
    }
    return 1;
}

/*
 * Draw the window containing the info text
 *
 */
static int sig_draw_window(xcb_window_t win, int width, int height, int font_height, i3String **crash_text_i3strings) {
    /* re-draw the background */
    xcb_rectangle_t border = {0, 0, width, height},
                    inner = {2, 2, width - 4, height - 4};
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){get_colorpixel("#FF0000")});
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){get_colorpixel("#000000")});
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &inner);

    /* restore font color */
    set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));

    char *bt_colour = "#FFFFFF";
    if (backtrace_done < 0)
        bt_colour = "#AA0000";
    else if (backtrace_done > 0)
        bt_colour = "#00AA00";

    for (int i = 0; crash_text_i3strings[i] != NULL; ++i) {
        /* fix the colour for the backtrace line when it finished */
        if (i == backtrace_string_index)
            set_font_colors(pixmap_gc, get_colorpixel(bt_colour), get_colorpixel("#000000"));

        draw_text(crash_text_i3strings[i], pixmap, pixmap_gc,
                  8, 5 + i * font_height, width - 16);

        /* and reset the colour again for other lines */
        if (i == backtrace_string_index)
            set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));
    }

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, width, height);
    xcb_flush(conn);

    return 1;
}

/*
 * Handles keypresses of 'b', 'r' and 'f' to get a backtrace or restart i3
 *
 */
static int sig_handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    uint16_t state = event->state;

    /* Apparantly, after activating numlock once, the numlock modifier
     * stays turned on (use xev(1) to verify). So, to resolve useful
     * keysyms, we remove the numlock flag from the event state */
    state &= ~xcb_numlock_mask;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(keysyms, event, state);

    if (sym == 'b') {
        DLOG("User issued core-dump command.\n");

        /* fork and exec/attach GDB to the parent to get a backtrace in the
         * tmpdir */
        backtrace_done = backtrace();

        /* re-open the windows to indicate that it's finished */
        open_popups();
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
                      win,                 /* the window id */
                      root,                /* parent == root */
                      x, y, width, height, /* dimensions */
                      0,                   /* border = 0, we draw our own */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
                      mask,
                      values);

    /* Map the window (= make it visible) */
    xcb_map_window(conn, win);

    return win;
}

static void open_popups() {
    /* width and height of the popup window, so that the text fits in */
    int crash_text_num = sizeof(crash_text) / sizeof(char *);
    int height = 13 + (crash_text_num * config.font.height);

    int crash_text_length = sizeof(crash_text) / sizeof(char *);
    i3String **crash_text_i3strings = smalloc(sizeof(i3String *) * (crash_text_length + 1));
    /* Pre-compute i3Strings for our text */
    for (int i = 0; i < crash_text_length; ++i) {
        crash_text_i3strings[i] = i3string_from_utf8(crash_text[i]);
    }
    crash_text_i3strings[crash_text_length] = NULL;
    /* calculate width for longest text */
    int font_width = predict_text_width(crash_text_i3strings[crash_text_longest]);
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

        /* Grab the keyboard to get all input */
        xcb_grab_keyboard(conn, false, win, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

        /* Grab the cursor inside the popup */
        xcb_grab_pointer(conn, false, win, XCB_NONE, XCB_GRAB_MODE_ASYNC,
                         XCB_GRAB_MODE_ASYNC, win, XCB_NONE, XCB_CURRENT_TIME);

        sig_draw_window(win, width, height, config.font.height, crash_text_i3strings);
        xcb_flush(conn);
    }
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

    open_popups();

    xcb_generic_event_t *event;
    /* Yay, more own eventhandlers… */
    while ((event = xcb_wait_for_event(conn))) {
        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);
        if (type == XCB_KEY_PRESS) {
            sig_handle_key_press(NULL, conn, (xcb_key_press_event_t *)event);
        }
        free(event);
    }
}

/*
 * Setup signal handlers to safely handle SIGSEGV and SIGFPE
 *
 */
void setup_signal_handler(void) {
    struct sigaction action;

    action.sa_sigaction = handle_signal;
    action.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
    sigemptyset(&action.sa_mask);

    /* Catch all signals with default action "Core", see signal(7) */
    if (sigaction(SIGQUIT, &action, NULL) == -1 ||
        sigaction(SIGILL, &action, NULL) == -1 ||
        sigaction(SIGABRT, &action, NULL) == -1 ||
        sigaction(SIGFPE, &action, NULL) == -1 ||
        sigaction(SIGSEGV, &action, NULL) == -1)
        ELOG("Could not setup signal handler");
}
