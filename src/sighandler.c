/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "all.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct dialog_t {
    xcb_window_t id;
    xcb_colormap_t colormap;
    Rect dims;
    surface_t surface;
    TAILQ_ENTRY(dialog_t) dialogs;
} dialog_t;

static TAILQ_HEAD(dialogs_head, dialog_t) dialogs = TAILQ_HEAD_INITIALIZER(dialogs);
static int raised_signal;
static int backtrace_done = 0;

static int sighandler_backtrace(void);
static void sighandler_setup(void);
static void sighandler_create_dialogs(void);
static void sighandler_destroy_dialogs(void);
static void sighandler_handle_expose(void);
static void sighandler_draw_dialog(dialog_t *dialog);
static void sighandler_handle_key_press(xcb_key_press_event_t *event);

static i3String *message_intro;
static i3String *message_intro2;
static i3String *message_option_backtrace;
static i3String *message_option_restart;
static i3String *message_option_forget;
static int dialog_width;
static int dialog_height;

static int border_width = 2;
static int margin = 4;

/*
 * Attach gdb to pid_parent and dump a backtrace to i3-backtrace.$pid in the
 * tmpdir
 */
static int sighandler_backtrace(void) {
    char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) {
        tmpdir = "/tmp";
    }

    pid_t pid_parent = getpid();

    char *filename = NULL;
    int suffix = 0;
    /* Find a unique filename for the backtrace (since the PID of i3 stays the
     * same), so that we don’t overwrite earlier backtraces. */
    do {
        FREE(filename);
        sasprintf(&filename, "%s/i3-backtrace.%d.%d.txt", tmpdir, pid_parent, suffix);
        suffix++;
    } while (path_exists(filename));

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
         * https://sourceware.org/bugzilla/show_bug.cgi?id=14114 */
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
        exit(EXIT_FAILURE);
    }
    int status = 0;

    waitpid(pid_gdb, &status, 0);

    /* see if the backtrace was successful or not */
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        DLOG("GDB did not run properly\n");
        return -1;
    } else if (!path_exists(filename)) {
        DLOG("GDB executed successfully, but no backtrace was generated\n");
        return -1;
    }
    return 1;
}

static void sighandler_setup(void) {
    border_width = logical_px(border_width);
    margin = logical_px(margin);

    int num_lines = 5;
    message_intro = i3string_from_utf8("i3 has just crashed. Please report a bug for this.");
    message_intro2 = i3string_from_utf8("To debug this problem, you can either attach gdb or choose from the following options:");
    message_option_backtrace = i3string_from_utf8("- 'b' to save a backtrace (requires gdb)");
    message_option_restart = i3string_from_utf8("- 'r' to restart i3 in-place");
    message_option_forget = i3string_from_utf8("- 'f' to forget the previous layout and restart i3");

    int width_longest_message = predict_text_width(message_intro2);

    dialog_width = width_longest_message + 2 * border_width + 2 * margin;
    dialog_height = num_lines * config.font.height + 2 * border_width + 2 * margin;
}

static void sighandler_create_dialogs(void) {
    Output *output;
    TAILQ_FOREACH (output, &outputs, outputs) {
        if (!output->active) {
            continue;
        }

        dialog_t *dialog = scalloc(1, sizeof(struct dialog_t));
        TAILQ_INSERT_TAIL(&dialogs, dialog, dialogs);

        xcb_visualid_t visual = get_visualid_by_depth(root_depth);
        dialog->colormap = xcb_generate_id(conn);
        xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, dialog->colormap, root, visual);

        uint32_t mask = 0;
        uint32_t values[4];
        int i = 0;

        /* Needs to be set in the case of a 32-bit root depth. */
        mask |= XCB_CW_BACK_PIXEL;
        values[i++] = root_screen->black_pixel;

        /* Needs to be set in the case of a 32-bit root depth. */
        mask |= XCB_CW_BORDER_PIXEL;
        values[i++] = root_screen->black_pixel;

        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[i++] = 1;

        /* Needs to be set in the case of a 32-bit root depth. */
        mask |= XCB_CW_COLORMAP;
        values[i++] = dialog->colormap;

        dialog->dims.x = output->rect.x + (output->rect.width / 2);
        dialog->dims.y = output->rect.y + (output->rect.height / 2);
        dialog->dims.width = dialog_width;
        dialog->dims.height = dialog_height;

        /* Make sure the dialog is centered. */
        dialog->dims.x -= dialog->dims.width / 2;
        dialog->dims.y -= dialog->dims.height / 2;

        dialog->id = create_window(conn, dialog->dims, root_depth, visual,
                                   XCB_WINDOW_CLASS_INPUT_OUTPUT, XCURSOR_CURSOR_POINTER,
                                   true, mask, values);

        draw_util_surface_init(conn, &(dialog->surface), dialog->id, get_visualtype_by_id(visual),
                               dialog->dims.width, dialog->dims.height);

        xcb_grab_keyboard(conn, false, dialog->id, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

        /* Confine the pointer to the crash dialog. */
        xcb_grab_pointer(conn, false, dialog->id, XCB_NONE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, dialog->id,
                         XCB_NONE, XCB_CURRENT_TIME);
    }

    sighandler_handle_expose();
    xcb_flush(conn);
}

static void sighandler_destroy_dialogs(void) {
    while (!TAILQ_EMPTY(&dialogs)) {
        dialog_t *dialog = TAILQ_FIRST(&dialogs);

        xcb_free_colormap(conn, dialog->colormap);
        draw_util_surface_free(conn, &(dialog->surface));
        xcb_destroy_window(conn, dialog->id);

        TAILQ_REMOVE(&dialogs, dialog, dialogs);
        free(dialog);
    }

    xcb_flush(conn);
}

static void sighandler_handle_expose(void) {
    dialog_t *current;
    TAILQ_FOREACH (current, &dialogs, dialogs) {
        sighandler_draw_dialog(current);
    }

    xcb_flush(conn);
}

static void sighandler_draw_dialog(dialog_t *dialog) {
    const color_t black = draw_util_hex_to_color("#000000");
    const color_t white = draw_util_hex_to_color("#FFFFFF");
    const color_t red = draw_util_hex_to_color("#FF0000");

    /* Start with a clean slate and draw a red border. */
    draw_util_clear_surface(&(dialog->surface), red);
    draw_util_rectangle(&(dialog->surface), black, border_width, border_width,
                        dialog->dims.width - 2 * border_width, dialog->dims.height - 2 * border_width);

    int y = border_width + margin;
    const int x = border_width + margin;
    const int max_width = dialog->dims.width - 2 * x;

    draw_util_text(message_intro, &(dialog->surface), white, black, x, y, max_width);
    y += config.font.height;

    draw_util_text(message_intro2, &(dialog->surface), white, black, x, y, max_width);
    y += config.font.height;

    char *bt_color = "#FFFFFF";
    if (backtrace_done < 0) {
        bt_color = "#AA0000";
    } else if (backtrace_done > 0) {
        bt_color = "#00AA00";
    }
    draw_util_text(message_option_backtrace, &(dialog->surface), draw_util_hex_to_color(bt_color), black, x, y, max_width);
    y += config.font.height;

    draw_util_text(message_option_restart, &(dialog->surface), white, black, x, y, max_width);
    y += config.font.height;

    draw_util_text(message_option_forget, &(dialog->surface), white, black, x, y, max_width);
    y += config.font.height;
}

static void sighandler_handle_key_press(xcb_key_press_event_t *event) {
    uint16_t state = event->state;

    /* Apparently, after activating numlock once, the numlock modifier
     * stays turned on (use xev(1) to verify). So, to resolve useful
     * keysyms, we remove the numlock flag from the event state */
    state &= ~xcb_numlock_mask;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(keysyms, event, state);

    if (sym == 'b') {
        DLOG("User issued core-dump command.\n");

        /* fork and exec/attach GDB to the parent to get a backtrace in the
         * tmpdir */
        backtrace_done = sighandler_backtrace();
        sighandler_handle_expose();
    } else if (sym == 'r') {
        sighandler_destroy_dialogs();
        i3_restart(false);
    } else if (sym == 'f') {
        sighandler_destroy_dialogs();
        i3_restart(true);
    }
}

static void handle_signal(int sig, siginfo_t *info, void *data) {
    DLOG("i3 crashed. SIG: %d\n", sig);

    struct sigaction action;
    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(sig, &action, NULL);
    raised_signal = sig;

    sighandler_setup();
    sighandler_create_dialogs();

    xcb_generic_event_t *event;
    /* Yay, more own eventhandlers… */
    while ((event = xcb_wait_for_event(conn))) {
        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);
        switch (type) {
            case XCB_KEY_PRESS:
                sighandler_handle_key_press((xcb_key_press_event_t *)event);
                break;
            case XCB_EXPOSE:
                if (((xcb_expose_event_t *)event)->count == 0) {
                    sighandler_handle_expose();
                }

                break;
        }

        free(event);
    }
}

/*
 * Configured a signal handler to gracefully handle crashes and allow the user
 * to generate a backtrace and rescue their session.
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
        sigaction(SIGSEGV, &action, NULL) == -1) {
        ELOG("Could not setup signal handler.\n");
    }
}
