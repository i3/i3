/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * child.c: Getting input for the statusline
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <ev.h>
#include <yajl/yajl_common.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>
#include <yajl/yajl_gen.h>
#include <paths.h>

#include "common.h"

/* Global variables for child_*() */
i3bar_child child;

/* stdin- and SIGCHLD-watchers */
ev_io *stdin_io;
ev_child *child_sig;

/* JSON parser for stdin */
yajl_handle parser;

/* JSON generator for stdout */
yajl_gen gen;

typedef struct parser_ctx {
    /* True if one of the parsed blocks was urgent */
    bool has_urgent;

    /* A copy of the last JSON map key. */
    char *last_map_key;

    /* The current block. Will be filled, then copied and put into the list of
     * blocks. */
    struct status_block block;
} parser_ctx;

parser_ctx parser_context;

struct statusline_head statusline_head = TAILQ_HEAD_INITIALIZER(statusline_head);
/* Used temporarily while reading a statusline */
struct statusline_head statusline_buffer = TAILQ_HEAD_INITIALIZER(statusline_buffer);

int child_stdin;

/*
 * Remove all blocks from the given statusline.
 * If free_resources is set, the fields of each status block will be free'd.
 */
static void clear_statusline(struct statusline_head *head, bool free_resources) {
    struct status_block *first;
    while (!TAILQ_EMPTY(head)) {
        first = TAILQ_FIRST(head);
        if (free_resources) {
            I3STRING_FREE(first->full_text);
            I3STRING_FREE(first->short_text);
            FREE(first->color);
            FREE(first->name);
            FREE(first->instance);
            FREE(first->min_width_str);
        }

        TAILQ_REMOVE(head, first, blocks);
        free(first);
    }
}

static void copy_statusline(struct statusline_head *from, struct statusline_head *to) {
    struct status_block *current;
    TAILQ_FOREACH(current, from, blocks) {
        struct status_block *new_block = smalloc(sizeof(struct status_block));
        memcpy(new_block, current, sizeof(struct status_block));
        TAILQ_INSERT_TAIL(to, new_block, blocks);
    }
}

/*
 * Replaces the statusline in memory with an error message. Pass a format
 * string and format parameters as you would in `printf'. The next time
 * `draw_bars' is called, the error message text will be drawn on the bar in
 * the space allocated for the statusline.
 */
__attribute__((format(printf, 1, 2))) static void set_statusline_error(const char *format, ...) {
    clear_statusline(&statusline_head, true);

    char *message;
    va_list args;
    va_start(args, format);
    (void)vasprintf(&message, format, args);

    struct status_block *err_block = scalloc(sizeof(struct status_block));
    err_block->full_text = i3string_from_utf8("Error: ");
    err_block->name = "error";
    err_block->color = "red";
    err_block->no_separator = true;

    struct status_block *message_block = scalloc(sizeof(struct status_block));
    message_block->full_text = i3string_from_utf8(message);
    message_block->name = "error_message";
    message_block->color = "red";
    message_block->no_separator = true;

    TAILQ_INSERT_HEAD(&statusline_head, err_block, blocks);
    TAILQ_INSERT_TAIL(&statusline_head, message_block, blocks);

    FREE(message);
    va_end(args);
}

/*
 * Stop and free() the stdin- and SIGCHLD-watchers
 *
 */
void cleanup(void) {
    if (stdin_io != NULL) {
        ev_io_stop(main_loop, stdin_io);
        FREE(stdin_io);
    }

    if (child_sig != NULL) {
        ev_child_stop(main_loop, child_sig);
        FREE(child_sig);
    }

    memset(&child, 0, sizeof(i3bar_child));
}

/*
 * The start of a new array is the start of a new status line, so we clear all
 * previous entries from the buffer.
 */
static int stdin_start_array(void *context) {
    // the blocks are still used by statusline_head, so we won't free the
    // resources here.
    clear_statusline(&statusline_buffer, false);
    return 1;
}

/*
 * The start of a map is the start of a single block of the status line.
 *
 */
static int stdin_start_map(void *context) {
    parser_ctx *ctx = context;
    memset(&(ctx->block), '\0', sizeof(struct status_block));

    /* Default width of the separator block. */
    if (config.separator_symbol == NULL)
        ctx->block.sep_block_width = logical_px(9);
    else
        ctx->block.sep_block_width = logical_px(8) + separator_symbol_width;

    return 1;
}

static int stdin_map_key(void *context, const unsigned char *key, size_t len) {
    parser_ctx *ctx = context;
    FREE(ctx->last_map_key);
    sasprintf(&(ctx->last_map_key), "%.*s", len, key);
    return 1;
}

static int stdin_boolean(void *context, int val) {
    parser_ctx *ctx = context;
    if (strcasecmp(ctx->last_map_key, "urgent") == 0) {
        ctx->block.urgent = val;
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "separator") == 0) {
        ctx->block.no_separator = !val;
        return 1;
    }

    return 1;
}

static int stdin_string(void *context, const unsigned char *val, size_t len) {
    parser_ctx *ctx = context;
    if (strcasecmp(ctx->last_map_key, "full_text") == 0) {
        ctx->block.full_text = i3string_from_markup_with_length((const char *)val, len);
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "short_text") == 0) {
        ctx->block.short_text = i3string_from_markup_with_length((const char *)val, len);
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "color") == 0) {
        sasprintf(&(ctx->block.color), "%.*s", len, val);
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "markup") == 0) {
        ctx->block.is_markup = (len == strlen("pango") && !strncasecmp((const char *)val, "pango", strlen("pango")));
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "align") == 0) {
        if (len == strlen("center") && !strncmp((const char *)val, "center", strlen("center"))) {
            ctx->block.align = ALIGN_CENTER;
        } else if (len == strlen("right") && !strncmp((const char *)val, "right", strlen("right"))) {
            ctx->block.align = ALIGN_RIGHT;
        } else {
            ctx->block.align = ALIGN_LEFT;
        }
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "min_width") == 0) {
        char *copy = (char *)malloc(len + 1);
        strncpy(copy, (const char *)val, len);
        copy[len] = 0;
        ctx->block.min_width_str = copy;
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "name") == 0) {
        char *copy = (char *)malloc(len + 1);
        strncpy(copy, (const char *)val, len);
        copy[len] = 0;
        ctx->block.name = copy;
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "instance") == 0) {
        char *copy = (char *)malloc(len + 1);
        strncpy(copy, (const char *)val, len);
        copy[len] = 0;
        ctx->block.instance = copy;
        return 1;
    }

    return 1;
}

static int stdin_integer(void *context, long long val) {
    parser_ctx *ctx = context;
    if (strcasecmp(ctx->last_map_key, "min_width") == 0) {
        ctx->block.min_width = (uint32_t)val;
        return 1;
    }
    if (strcasecmp(ctx->last_map_key, "separator_block_width") == 0) {
        ctx->block.sep_block_width = (uint32_t)val;
        return 1;
    }

    return 1;
}

/*
 * When a map is finished, we have an entire status block.
 * Move it from the parser's context to the statusline buffer.
 */
static int stdin_end_map(void *context) {
    parser_ctx *ctx = context;
    struct status_block *new_block = smalloc(sizeof(struct status_block));
    memcpy(new_block, &(ctx->block), sizeof(struct status_block));
    /* Ensure we have a full_text set, so that when it is missing (or null),
     * i3bar doesn’t crash and the user gets an annoying message. */
    if (!new_block->full_text)
        new_block->full_text = i3string_from_utf8("SPEC VIOLATION: full_text is NULL!");
    if (new_block->urgent)
        ctx->has_urgent = true;

    if (new_block->min_width_str) {
        i3String *text = i3string_from_utf8(new_block->min_width_str);
        i3string_set_markup(text, new_block->is_markup);
        new_block->min_width = (uint32_t)predict_text_width(text);
        i3string_free(text);
    }

    i3string_set_markup(new_block->full_text, new_block->is_markup);

    if (new_block->short_text != NULL)
        i3string_set_markup(new_block->short_text, new_block->is_markup);

    TAILQ_INSERT_TAIL(&statusline_buffer, new_block, blocks);
    return 1;
}

/*
 * When an array is finished, we have an entire statusline.
 * Copy it from the buffer to the actual statusline.
 */
static int stdin_end_array(void *context) {
    DLOG("copying statusline_buffer to statusline_head\n");
    clear_statusline(&statusline_head, true);
    copy_statusline(&statusline_buffer, &statusline_head);

    DLOG("dumping statusline:\n");
    struct status_block *current;
    TAILQ_FOREACH(current, &statusline_head, blocks) {
        DLOG("full_text = %s\n", i3string_as_utf8(current->full_text));
        DLOG("short_text = %s\n", (current->short_text == NULL ? NULL : i3string_as_utf8(current->short_text)));
        DLOG("color = %s\n", current->color);
    }
    DLOG("end of dump\n");
    return 1;
}

/*
 * Helper function to read stdin
 *
 * Returns NULL on EOF.
 *
 */
static unsigned char *get_buffer(ev_io *watcher, int *ret_buffer_len) {
    int fd = watcher->fd;
    int n = 0;
    int rec = 0;
    int buffer_len = STDIN_CHUNK_SIZE;
    unsigned char *buffer = smalloc(buffer_len + 1);
    buffer[0] = '\0';
    while (1) {
        n = read(fd, buffer + rec, buffer_len - rec);
        if (n == -1) {
            if (errno == EAGAIN) {
                /* finish up */
                break;
            }
            ELOG("read() failed!: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            ELOG("stdin: received EOF\n");
            *ret_buffer_len = -1;
            return NULL;
        }
        rec += n;

        if (rec == buffer_len) {
            buffer_len += STDIN_CHUNK_SIZE;
            buffer = srealloc(buffer, buffer_len);
        }
    }
    if (*buffer == '\0') {
        FREE(buffer);
        rec = -1;
    }
    *ret_buffer_len = rec;
    return buffer;
}

static void read_flat_input(char *buffer, int length) {
    struct status_block *first = TAILQ_FIRST(&statusline_head);
    /* Clear the old buffer if any. */
    I3STRING_FREE(first->full_text);
    /* Remove the trailing newline and terminate the string at the same
     * time. */
    if (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')
        buffer[length - 1] = '\0';
    else
        buffer[length] = '\0';
    first->full_text = i3string_from_markup(buffer);
}

static bool read_json_input(unsigned char *input, int length) {
    yajl_status status = yajl_parse(parser, input, length);
    bool has_urgent = false;
    if (status != yajl_status_ok) {
        char *message = (char *)yajl_get_error(parser, 0, input, length);

        /* strip the newline yajl adds to the error message */
        if (message[strlen(message) - 1] == '\n')
            message[strlen(message) - 1] = '\0';

        fprintf(stderr, "[i3bar] Could not parse JSON input (code = %d, message = %s): %.*s\n",
                status, message, length, input);

        set_statusline_error("Could not parse JSON (%s)", message);
        yajl_free_error(parser, (unsigned char *)message);
        draw_bars(false);
    } else if (parser_context.has_urgent) {
        has_urgent = true;
    }
    return has_urgent;
}

/*
 * Callbalk for stdin. We read a line from stdin and store the result
 * in statusline
 *
 */
void stdin_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    int rec;
    unsigned char *buffer = get_buffer(watcher, &rec);
    if (buffer == NULL)
        return;
    bool has_urgent = false;
    if (child.version > 0) {
        has_urgent = read_json_input(buffer, rec);
    } else {
        read_flat_input((char *)buffer, rec);
    }
    free(buffer);
    draw_bars(has_urgent);
}

/*
 * Callbalk for stdin first line. We read the first line to detect
 * whether this is JSON or plain text
 *
 */
void stdin_io_first_line_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    int rec;
    unsigned char *buffer = get_buffer(watcher, &rec);
    if (buffer == NULL)
        return;
    DLOG("Detecting input type based on buffer *%.*s*\n", rec, buffer);
    /* Detect whether this is JSON or plain text. */
    unsigned int consumed = 0;
    /* At the moment, we don’t care for the version. This might change
     * in the future, but for now, we just discard it. */
    parse_json_header(&child, buffer, rec, &consumed);
    if (child.version > 0) {
        /* If hide-on-modifier is set, we start of by sending the
         * child a SIGSTOP, because the bars aren't mapped at start */
        if (config.hide_on_modifier) {
            stop_child();
        }
        draw_bars(read_json_input(buffer + consumed, rec - consumed));
    } else {
        /* In case of plaintext, we just add a single block and change its
         * full_text pointer later. */
        struct status_block *new_block = scalloc(sizeof(struct status_block));
        TAILQ_INSERT_TAIL(&statusline_head, new_block, blocks);
        read_flat_input((char *)buffer, rec);
    }
    free(buffer);
    ev_io_stop(main_loop, stdin_io);
    ev_io_init(stdin_io, &stdin_io_cb, STDIN_FILENO, EV_READ);
    ev_io_start(main_loop, stdin_io);
}

/*
 * We received a SIGCHLD, meaning, that the child process terminated.
 * We simply free the respective data structures and don't care for input
 * anymore
 *
 */
void child_sig_cb(struct ev_loop *loop, ev_child *watcher, int revents) {
    int exit_status = WEXITSTATUS(watcher->rstatus);

    ELOG("Child (pid: %d) unexpectedly exited with status %d\n",
         child.pid,
         exit_status);

    /* this error is most likely caused by a user giving a nonexecutable or
     * nonexistent file, so we will handle those cases separately. */
    if (exit_status == 126)
        set_statusline_error("status_command is not executable (exit %d)", exit_status);
    else if (exit_status == 127)
        set_statusline_error("status_command not found or is missing a library dependency (exit %d)", exit_status);
    else
        set_statusline_error("status_command process exited unexpectedly (exit %d)", exit_status);

    cleanup();
    draw_bars(false);
}

void child_write_output(void) {
    if (child.click_events) {
        const unsigned char *output;
        size_t size;
        ssize_t n;

        yajl_gen_get_buf(gen, &output, &size);

        n = writeall(child_stdin, output, size);
        if (n != -1)
            n = writeall(child_stdin, "\n", 1);

        yajl_gen_clear(gen);

        if (n == -1) {
            child.click_events = false;
            kill_child();
            set_statusline_error("child_write_output failed");
            draw_bars(false);
        }
    }
}

/*
 * Start a child process with the specified command and reroute stdin.
 * We actually start a $SHELL to execute the command so we don't have to care
 * about arguments and such.
 *
 * If `command' is NULL, such as in the case when no `status_command' is given
 * in the bar config, no child will be started.
 *
 */
void start_child(char *command) {
    if (command == NULL)
        return;

    /* Allocate a yajl parser which will be used to parse stdin. */
    static yajl_callbacks callbacks = {
        .yajl_boolean = stdin_boolean,
        .yajl_integer = stdin_integer,
        .yajl_string = stdin_string,
        .yajl_start_map = stdin_start_map,
        .yajl_map_key = stdin_map_key,
        .yajl_end_map = stdin_end_map,
        .yajl_start_array = stdin_start_array,
        .yajl_end_array = stdin_end_array,
    };
    parser = yajl_alloc(&callbacks, NULL, &parser_context);

    gen = yajl_gen_alloc(NULL);

    int pipe_in[2];  /* pipe we read from */
    int pipe_out[2]; /* pipe we write to */

    if (pipe(pipe_in) == -1)
        err(EXIT_FAILURE, "pipe(pipe_in)");
    if (pipe(pipe_out) == -1)
        err(EXIT_FAILURE, "pipe(pipe_out)");

    child.pid = fork();
    switch (child.pid) {
        case -1:
            ELOG("Couldn't fork(): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        case 0:
            /* Child-process. Reroute streams and start shell */

            close(pipe_in[0]);
            close(pipe_out[1]);

            dup2(pipe_in[1], STDOUT_FILENO);
            dup2(pipe_out[0], STDIN_FILENO);

            setpgid(child.pid, 0);
            execl(_PATH_BSHELL, _PATH_BSHELL, "-c", command, (char *)NULL);
            return;
        default:
            /* Parent-process. Reroute streams */

            close(pipe_in[1]);
            close(pipe_out[0]);

            dup2(pipe_in[0], STDIN_FILENO);
            child_stdin = pipe_out[1];

            break;
    }

    /* We set O_NONBLOCK because blocking is evil in event-driven software */
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    stdin_io = smalloc(sizeof(ev_io));
    ev_io_init(stdin_io, &stdin_io_first_line_cb, STDIN_FILENO, EV_READ);
    ev_io_start(main_loop, stdin_io);

    /* We must cleanup, if the child unexpectedly terminates */
    child_sig = smalloc(sizeof(ev_child));
    ev_child_init(child_sig, &child_sig_cb, child.pid, 0);
    ev_child_start(main_loop, child_sig);

    atexit(kill_child_at_exit);
}

void child_click_events_initialize(void) {
    if (!child.click_events_init) {
        yajl_gen_array_open(gen);
        child_write_output();
        child.click_events_init = true;
    }
}

void child_click_events_key(const char *key) {
    yajl_gen_string(gen, (const unsigned char *)key, strlen(key));
}

/*
 * Generates a click event, if enabled.
 *
 */
void send_block_clicked(int button, const char *name, const char *instance, int x, int y) {
    if (!child.click_events) {
        return;
    }

    child_click_events_initialize();

    yajl_gen_map_open(gen);

    if (name) {
        child_click_events_key("name");
        yajl_gen_string(gen, (const unsigned char *)name, strlen(name));
    }

    if (instance) {
        child_click_events_key("instance");
        yajl_gen_string(gen, (const unsigned char *)instance, strlen(instance));
    }

    child_click_events_key("button");
    yajl_gen_integer(gen, button);

    child_click_events_key("x");
    yajl_gen_integer(gen, x);

    child_click_events_key("y");
    yajl_gen_integer(gen, y);

    yajl_gen_map_close(gen);
    child_write_output();
}

/*
 * kill()s the child process (if any). Called when exit()ing.
 *
 */
void kill_child_at_exit(void) {
    if (child.pid > 0) {
        if (child.cont_signal > 0 && child.stopped)
            killpg(child.pid, child.cont_signal);
        killpg(child.pid, SIGTERM);
    }
}

/*
 * kill()s the child process (if existent) and closes and
 * free()s the stdin- and SIGCHLD-watchers
 *
 */
void kill_child(void) {
    if (child.pid > 0) {
        if (child.cont_signal > 0 && child.stopped)
            killpg(child.pid, child.cont_signal);
        killpg(child.pid, SIGTERM);
        int status;
        waitpid(child.pid, &status, 0);
        cleanup();
    }
}

/*
 * Sends a SIGSTOP to the child process (if existent)
 *
 */
void stop_child(void) {
    if (child.stop_signal > 0 && !child.stopped) {
        child.stopped = true;
        killpg(child.pid, child.stop_signal);
    }
}

/*
 * Sends a SIGCONT to the child process (if existent)
 *
 */
void cont_child(void) {
    if (child.cont_signal > 0 && child.stopped) {
        child.stopped = false;
        killpg(child.pid, child.cont_signal);
    }
}

/*
 * Whether or not the child want click events
 *
 */
bool child_want_click_events(void) {
    return child.click_events;
}
