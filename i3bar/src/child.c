/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * child.c: Getting input for the statusline
 *
 */
#include "common.h"
#include "queue.h"

#include <ctype.h> /* isspace */
#include <err.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include <yyjson.h>

/* Global variables for child_*() */
i3bar_child status_child = {0};
i3bar_child ws_child = {0};

#define DLOG_CHILD(c)                                                                                                              \
    do {                                                                                                                           \
        if ((c).pid == 0) {                                                                                                        \
            DLOG("%s: child pid = 0\n", __func__);                                                                                 \
        } else if ((c).pid == status_child.pid) {                                                                                  \
            DLOG("%s: status_command: pid=%ld stopped=%d stop_signal=%d cont_signal=%d click_events=%d click_events_init=%d\n",    \
                 __func__, (long)(c).pid, (c).stopped, (c).stop_signal, (c).cont_signal, (c).click_events, (c).click_events_init); \
        } else if ((c).pid == ws_child.pid) {                                                                                      \
            DLOG("%s: workspace_command: pid=%ld stopped=%d stop_signal=%d cont_signal=%d click_events=%d click_events_init=%d\n", \
                 __func__, (long)(c).pid, (c).stopped, (c).stop_signal, (c).cont_signal, (c).click_events, (c).click_events_init); \
        } else {                                                                                                                   \
            ELOG("%s: unknown child, this should never happen "                                                                    \
                 "pid=%ld stopped=%d stop_signal=%d cont_signal=%d click_events=%d click_events_init=%d\n",                        \
                 __func__, (long)(c).pid, (c).stopped, (c).stop_signal, (c).cont_signal, (c).click_events, (c).click_events_init); \
        }                                                                                                                          \
    } while (0)
#define DLOG_CHILDREN             \
    do {                          \
        DLOG_CHILD(status_child); \
        DLOG_CHILD(ws_child);     \
    } while (0)

struct statusline_head statusline_head = TAILQ_HEAD_INITIALIZER(statusline_head);
/* Used temporarily while reading a statusline */
struct statusline_head statusline_buffer = TAILQ_HEAD_INITIALIZER(statusline_buffer);

int child_stdin;

/*
 * Remove all blocks from the given statusline.
 * If free_resources is set, the fields of each status block will be free'd.
 */
void clear_statusline(struct statusline_head *head, bool free_resources) {
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
            FREE(first->background);
            FREE(first->border);
        }

        TAILQ_REMOVE(head, first, blocks);
        free(first);
    }
}

static void copy_statusline(struct statusline_head *from, struct statusline_head *to) {
    struct status_block *current;
    TAILQ_FOREACH (current, from, blocks) {
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
    if (vasprintf(&message, format, args) == -1) {
        goto finish;
    }

    struct status_block *err_block = scalloc(1, sizeof(struct status_block));
    err_block->full_text = i3string_from_utf8("Error: ");
    err_block->name = sstrdup("error");
    err_block->color = sstrdup("#ff0000");
    err_block->no_separator = true;

    struct status_block *message_block = scalloc(1, sizeof(struct status_block));
    message_block->full_text = i3string_from_utf8(message);
    message_block->name = sstrdup("error_message");
    message_block->color = sstrdup("#ff0000");
    message_block->no_separator = true;

    TAILQ_INSERT_HEAD(&statusline_head, err_block, blocks);
    TAILQ_INSERT_TAIL(&statusline_head, message_block, blocks);

finish:
    free(message);
    va_end(args);
}

/*
 * Stop and free() the stdin- and SIGCHLD-watchers
 *
 */
static void cleanup(i3bar_child *c) {
    DLOG_CHILD(*c);

    if (c->stdin_io != NULL) {
        ev_io_stop(main_loop, c->stdin_io);
        FREE(c->stdin_io);

        if (c->pid == status_child.pid) {
            close(child_stdin);
            child_stdin = 0;
        }
        close(c->stdin_fd);
    }

    if (c->child_sig != NULL) {
        ev_child_stop(main_loop, c->child_sig);
        FREE(c->child_sig);
    }

    FREE(c->pending_line);
    memset(c, 0, sizeof(i3bar_child));
}

/*
 * Parse a single status block from JSON
 */
static void parse_status_block(yyjson_val *block_obj, bool *has_urgent) {
    if (!yyjson_is_obj(block_obj)) {
        return;
    }

    struct status_block block = {0};

    /* Default width of the separator block. */
    if (config.separator_symbol == NULL) {
        block.sep_block_width = logical_px(9);
    } else {
        block.sep_block_width = logical_px(8) + separator_symbol_width;
    }

    /* By default we draw all four borders if a border is set. */
    block.border_top = 1;
    block.border_right = 1;
    block.border_bottom = 1;
    block.border_left = 1;

    /* Parse string fields */
    yyjson_val *full_text = yyjson_obj_get(block_obj, "full_text");
    if (full_text && yyjson_is_str(full_text)) {
        block.full_text = i3string_from_markup_with_length(
            yyjson_get_str(full_text), yyjson_get_len(full_text));
    }

    yyjson_val *short_text = yyjson_obj_get(block_obj, "short_text");
    if (short_text && yyjson_is_str(short_text)) {
        block.short_text = i3string_from_markup_with_length(
            yyjson_get_str(short_text), yyjson_get_len(short_text));
    }

    yyjson_val *color = yyjson_obj_get(block_obj, "color");
    if (color && yyjson_is_str(color)) {
        block.color = sstrdup(yyjson_get_str(color));
    }

    yyjson_val *background = yyjson_obj_get(block_obj, "background");
    if (background && yyjson_is_str(background)) {
        block.background = sstrdup(yyjson_get_str(background));
    }

    yyjson_val *border = yyjson_obj_get(block_obj, "border");
    if (border && yyjson_is_str(border)) {
        block.border = sstrdup(yyjson_get_str(border));
    }

    yyjson_val *name = yyjson_obj_get(block_obj, "name");
    if (name && yyjson_is_str(name)) {
        block.name = sstrdup(yyjson_get_str(name));
    }

    yyjson_val *instance = yyjson_obj_get(block_obj, "instance");
    if (instance && yyjson_is_str(instance)) {
        block.instance = sstrdup(yyjson_get_str(instance));
    }

    yyjson_val *min_width_val = yyjson_obj_get(block_obj, "min_width");
    if (min_width_val) {
        if (yyjson_is_str(min_width_val)) {
            block.min_width_str = sstrdup(yyjson_get_str(min_width_val));
        } else if (yyjson_is_int(min_width_val)) {
            block.min_width = yyjson_get_int(min_width_val);
        }
    }

    yyjson_val *markup = yyjson_obj_get(block_obj, "markup");
    if (markup && yyjson_is_str(markup)) {
        const char *m = yyjson_get_str(markup);
        block.pango_markup = (strcasecmp(m, "pango") == 0);
    }

    yyjson_val *align = yyjson_obj_get(block_obj, "align");
    if (align && yyjson_is_str(align)) {
        const char *a = yyjson_get_str(align);
        if (strcmp(a, "center") == 0) {
            block.align = ALIGN_CENTER;
        } else if (strcmp(a, "right") == 0) {
            block.align = ALIGN_RIGHT;
        } else {
            block.align = ALIGN_LEFT;
        }
    }

    /* Parse boolean fields */
    yyjson_val *urgent = yyjson_obj_get(block_obj, "urgent");
    if (urgent && yyjson_is_bool(urgent)) {
        block.urgent = yyjson_get_bool(urgent);
    }

    yyjson_val *separator = yyjson_obj_get(block_obj, "separator");
    if (separator && yyjson_is_bool(separator)) {
        block.no_separator = !yyjson_get_bool(separator);
    }

    /* Parse integer fields */
    yyjson_val *sep_width = yyjson_obj_get(block_obj, "separator_block_width");
    if (sep_width && yyjson_is_int(sep_width)) {
        block.sep_block_width = yyjson_get_int(sep_width);
    }

    yyjson_val *border_top = yyjson_obj_get(block_obj, "border_top");
    if (border_top && yyjson_is_int(border_top)) {
        block.border_top = yyjson_get_int(border_top);
    }

    yyjson_val *border_right = yyjson_obj_get(block_obj, "border_right");
    if (border_right && yyjson_is_int(border_right)) {
        block.border_right = yyjson_get_int(border_right);
    }

    yyjson_val *border_bottom = yyjson_obj_get(block_obj, "border_bottom");
    if (border_bottom && yyjson_is_int(border_bottom)) {
        block.border_bottom = yyjson_get_int(border_bottom);
    }

    yyjson_val *border_left = yyjson_obj_get(block_obj, "border_left");
    if (border_left && yyjson_is_int(border_left)) {
        block.border_left = yyjson_get_int(border_left);
    }

    /* Create the new block */
    struct status_block *new_block = smalloc(sizeof(struct status_block));
    memcpy(new_block, &block, sizeof(struct status_block));

    /* Ensure we have a full_text set */
    if (!new_block->full_text) {
        new_block->full_text = i3string_from_utf8("SPEC VIOLATION: full_text is NULL!");
    }

    if (new_block->urgent) {
        *has_urgent = true;
    }

    if (new_block->min_width_str) {
        i3String *text = i3string_from_utf8(new_block->min_width_str);
        i3string_set_markup(text, new_block->pango_markup);
        new_block->min_width = (uint32_t)predict_text_width(text);
        i3string_free(text);
    }

    i3string_set_markup(new_block->full_text, new_block->pango_markup);

    new_block->use_short = false;
    if (new_block->short_text != NULL) {
        i3string_set_markup(new_block->short_text, new_block->pango_markup);
    }

    TAILQ_INSERT_TAIL(&statusline_buffer, new_block, blocks);
}

/*
 * Helper function to read stdin
 *
 * Returns NULL on EOF.
 *
 */
static unsigned char *get_buffer(int fd, int *ret_buffer_len) {
    int rec = 0;
    int buffer_len = STDIN_CHUNK_SIZE;
    unsigned char *buffer = smalloc(buffer_len + 1);
    buffer[0] = '\0';
    while (1) {
        const ssize_t n = read(fd, buffer + rec, buffer_len - rec);
        if (n == -1) {
            if (errno == EAGAIN) {
                /* finish up */
                break;
            }
            ELOG("read() failed!: %s\n", strerror(errno));
            FREE(buffer);
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            ELOG("stdin: received EOF\n");
            FREE(buffer);
            *ret_buffer_len = -1;
            return NULL;
        }
        rec += n;

        if (rec == buffer_len) {
            buffer_len += STDIN_CHUNK_SIZE;
            buffer = srealloc(buffer, buffer_len + 1);
        }
    }
    buffer[rec] = '\0';
    if (buffer[0] == '\0') {
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
    if (buffer[length - 1] == '\n' || buffer[length - 1] == '\r') {
        buffer[length - 1] = '\0';
    } else {
        buffer[length] = '\0';
    }

    first->full_text = i3string_from_utf8(buffer);
}

static bool read_json_input(unsigned char *input, int length) {
    bool has_urgent = false;

    yyjson_doc *doc = yyjson_read((const char *)input, length, 0);
    if (!doc) {
        fprintf(stderr, "[i3bar] Could not parse JSON input: %.*s\n", length, input);
        set_statusline_error("Could not parse JSON");
        draw_bars(false);
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) {
        fprintf(stderr, "[i3bar] JSON input is not an array: %.*s\n", length, input);
        set_statusline_error("JSON input is not an array");
        yyjson_doc_free(doc);
        draw_bars(false);
        return false;
    }

    /* Clear the buffer for the new statusline */
    clear_statusline(&statusline_buffer, false);

    /* Parse each block */
    size_t idx, max;
    yyjson_val *block_obj;
    yyjson_arr_foreach(root, idx, max, block_obj) {
        parse_status_block(block_obj, &has_urgent);
    }

    /* Copy buffer to actual statusline */
    DLOG("copying statusline_buffer to statusline_head\n");
    clear_statusline(&statusline_head, true);
    copy_statusline(&statusline_buffer, &statusline_head);

    DLOG("dumping statusline:\n");
    struct status_block *current;
    TAILQ_FOREACH (current, &statusline_head, blocks) {
        DLOG("full_text = %s\n", i3string_as_utf8(current->full_text));
        DLOG("short_text = %s\n", (current->short_text == NULL ? NULL : i3string_as_utf8(current->short_text)));
        DLOG("color = %s\n", current->color);
    }
    DLOG("end of dump\n");

    yyjson_doc_free(doc);
    return has_urgent;
}

/*
 * Callbalk for stdin. We read a line from stdin and store the result
 * in statusline
 *
 */
static void stdin_io_cb(int fd) {
    int rec;
    unsigned char *buffer = get_buffer(fd, &rec);
    if (buffer == NULL) {
        return;
    }
    bool has_urgent = false;
    if (status_child.version > 0) {
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
static void stdin_io_first_line_cb(int fd) {
    int rec;
    unsigned char *buffer = get_buffer(fd, &rec);
    if (buffer == NULL) {
        return;
    }
    DLOG("Detecting input type based on buffer *%.*s*\n", rec, buffer);
    /* Detect whether this is JSON or plain text. */
    unsigned int consumed = 0;
    /* At the moment, we don't care for the version. This might change
     * in the future, but for now, we just discard it. */
    parse_json_header(&status_child, buffer, rec, &consumed);
    if (status_child.version > 0) {
        /* If hide-on-modifier is set, we start of by sending the status_child
         * a SIGSTOP, because the bars aren't mapped at start */
        if (config.hide_on_modifier) {
            stop_children();
        }
        draw_bars(read_json_input(buffer + consumed, rec - consumed));
    } else {
        /* In case of plaintext, we just add a single block and change its
         * full_text pointer later. */
        struct status_block *new_block = scalloc(1, sizeof(struct status_block));
        TAILQ_INSERT_TAIL(&statusline_head, new_block, blocks);
        read_flat_input((char *)buffer, rec);
    }
    free(buffer);
}

static bool isempty(char *s) {
    while (*s != '\0') {
        if (!isspace(*s)) {
            return false;
        }
        s++;
    }
    return true;
}

static char *append_string(const char *previous, const char *str) {
    if (previous != NULL) {
        char *result;
        sasprintf(&result, "%s%s", previous, str);
        return result;
    }
    return sstrdup(str);
}

static char *ws_last_json;

static void ws_stdin_io_cb(int fd) {
    int rec;
    unsigned char *buffer = get_buffer(fd, &rec);
    if (buffer == NULL) {
        return;
    }

    gchar **strings = g_strsplit((const char *)buffer, "\n", 0);
    for (int idx = 0; strings[idx] != NULL; idx++) {
        if (ws_child.pending_line == NULL && isempty(strings[idx])) {
            /* In the normal case where the buffer ends with '\n', the last
             * string should be empty */
            continue;
        }

        if (strings[idx + 1] == NULL) {
            /* This is the last string but it is not empty, meaning that we have
             * read data that is incomplete, save it for later. */
            char *new = append_string(ws_child.pending_line, strings[idx]);
            free(ws_child.pending_line);
            ws_child.pending_line = new;
            continue;
        }

        free(ws_last_json);
        ws_last_json = append_string(ws_child.pending_line, strings[idx]);
        FREE(ws_child.pending_line);

        parse_workspaces_json((const unsigned char *)ws_last_json, strlen(ws_last_json));
    }

    g_strfreev(strings);
    free(buffer);

    draw_bars(false);
}

static void common_stdin_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    if (watcher == status_child.stdin_io) {
        if (status_child.version == (uint32_t)-1) {
            stdin_io_first_line_cb(watcher->fd);
        } else {
            stdin_io_cb(watcher->fd);
        }
    } else if (watcher == ws_child.stdin_io) {
        ws_stdin_io_cb(watcher->fd);
    } else {
        ELOG("Got callback for unknown watcher fd=%d\n", watcher->fd);
    }
}

/*
 * When workspace_command is enabled this function is used to re-parse the
 * latest received JSON from the client.
 */
void repeat_last_ws_json(void) {
    if (ws_last_json) {
        DLOG("Repeating last workspace JSON\n");
        parse_workspaces_json((const unsigned char *)ws_last_json, strlen(ws_last_json));
    }
}

/*
 * Wrapper around set_workspace_button_error to mimic the call of
 * set_statusline_error.
 */
__attribute__((format(printf, 1, 2))) static void set_workspace_button_error_f(const char *format, ...) {
    char *message;
    va_list args;
    va_start(args, format);
    if (vasprintf(&message, format, args) == -1) {
        goto finish;
    }

    set_workspace_button_error(message);

finish:
    free(message);
    va_end(args);
}

/*
 * Replaces the workspace buttons with an error message.
 */
void set_workspace_button_error(const char *message) {
    free_workspaces();

    char *name = NULL;
    sasprintf(&name, "Error: %s", message);

    i3_output *output;
    SLIST_FOREACH (output, outputs, slist) {
        i3_ws *fake_ws = scalloc(1, sizeof(i3_ws));
        /* Don't set the canonical_name field to make this workspace unfocusable. */
        fake_ws->name = i3string_from_utf8(name);
        fake_ws->name_width = predict_text_width(fake_ws->name);
        fake_ws->num = -1;
        fake_ws->urgent = fake_ws->visible = true;
        fake_ws->output = output;

        TAILQ_INSERT_TAIL(output->workspaces, fake_ws, tailq);
    }

    free(name);
}

/*
 * We received a SIGCHLD, meaning, that the child process terminated.
 * We simply free the respective data structures and don't care for input
 * anymore
 *
 */
static void child_sig_cb(struct ev_loop *loop, ev_child *watcher, int revents) {
    const int exit_status = WEXITSTATUS(watcher->rstatus);

    ELOG("Child (pid: %d) unexpectedly exited with status %d\n",
         watcher->pid,
         exit_status);

    __attribute__((format(printf, 1, 2))) void (*error_function_pointer)(const char *, ...) = NULL;
    const char *command_type = "";
    i3bar_child *c = NULL;
    if (watcher->pid == status_child.pid) {
        command_type = "status_command";
        error_function_pointer = set_statusline_error;
        c = &status_child;
    } else if (watcher->pid == ws_child.pid) {
        command_type = "workspace_command";
        error_function_pointer = set_workspace_button_error_f;
        c = &ws_child;
    } else {
        ELOG("Unknown child pid, this should never happen\n");
        return;
    }
    DLOG_CHILD(*c);

    /* this error is most likely caused by a user giving a nonexecutable or
     * nonexistent file, so we will handle those cases separately. */
    if (exit_status == 126) {
        error_function_pointer("%s is not executable (exit %d)", command_type, exit_status);
    } else if (exit_status == 127) {
        error_function_pointer("%s not found or is missing a library dependency (exit %d)", command_type, exit_status);
    } else {
        error_function_pointer("%s process exited unexpectedly (exit %d)", command_type, exit_status);
    }

    cleanup(c);
    draw_bars(false);
}


static pid_t sfork(void) {
    const pid_t pid = fork();
    if (pid == -1) {
        ELOG("Couldn't fork(): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return pid;
}

static void spipe(int pipedes[2]) {
    if (pipe(pipedes) == -1) {
        err(EXIT_FAILURE, "pipe(pipe_in)");
    }
}

static void exec_shell(char *command) {
    execl(_PATH_BSHELL, _PATH_BSHELL, "-c", command, (char *)NULL);
    err(EXIT_FAILURE, "execl return"); /* only reached on error */
}

static void setup_child_cb(i3bar_child *child) {
    /* We set O_NONBLOCK because blocking is evil in event-driven software */
    fcntl(child->stdin_fd, F_SETFL, O_NONBLOCK);

    child->stdin_io = smalloc(sizeof(ev_io));
    ev_io_init(child->stdin_io, &common_stdin_cb, child->stdin_fd, EV_READ);
    ev_io_start(main_loop, child->stdin_io);

    /* We must cleanup, if the child unexpectedly terminates */
    child->child_sig = smalloc(sizeof(ev_child));
    ev_child_init(child->child_sig, &child_sig_cb, child->pid, 0);
    ev_child_start(main_loop, child->child_sig);

    DLOG_CHILD(*child);
}

/*
 * Start a child process with the specified command and reroute stdin.
 * We actually start a shell to execute the command so we don't have to care
 * about arguments and such.
 *
 * If `command' is NULL, such as in the case when no `status_command' is given
 * in the bar config, no child will be started.
 *
 */
void start_child(char *command) {
    if (command == NULL) {
        return;
    }

    int pipe_in[2];  /* pipe we read from */
    int pipe_out[2]; /* pipe we write to */
    spipe(pipe_in);
    spipe(pipe_out);

    status_child.pid = sfork();
    if (status_child.pid == 0) {
        /* Child-process. Reroute streams and start shell */
        close(pipe_in[0]);
        close(pipe_out[1]);

        dup2(pipe_in[1], STDOUT_FILENO);
        dup2(pipe_out[0], STDIN_FILENO);

        setpgid(status_child.pid, 0);
        exec_shell(command);
        return;
    }
    /* Parent-process. Reroute streams */
    close(pipe_in[1]);
    close(pipe_out[0]);

    status_child.stdin_fd = pipe_in[0];
    child_stdin = pipe_out[1];
    status_child.version = -1;

    setup_child_cb(&status_child);
}

/*
 * Same as start_child but starts the configured client that manages workspace
 * buttons.
 *
 */
void start_ws_child(char *command) {
    if (command == NULL) {
        return;
    }

    ws_child.stop_signal = SIGSTOP;
    ws_child.cont_signal = SIGCONT;

    int pipe_in[2]; /* pipe we read from */
    spipe(pipe_in);

    ws_child.pid = sfork();
    if (ws_child.pid == 0) {
        /* Child-process. Reroute streams and start shell */
        close(pipe_in[0]);
        dup2(pipe_in[1], STDOUT_FILENO);

        setpgid(ws_child.pid, 0);
        exec_shell(command);
    }
    /* Parent-process. Reroute streams */
    close(pipe_in[1]);
    ws_child.stdin_fd = pipe_in[0];

    setup_child_cb(&ws_child);
}

static void child_click_events_initialize(void) {
    DLOG_CHILD(status_child);

    if (!status_child.click_events_init) {
        /* Write opening bracket */
        ssize_t n = writeall(child_stdin, "[", 1);
        if (n != -1) {
            n = writeall(child_stdin, "\n", 1);
        }
        if (n == -1) {
            status_child.click_events = false;
            return;
        }
        status_child.click_events_init = true;
    }
}

/*
 * Generates a click event, if enabled.
 *
 */
void send_block_clicked(int button, const char *name, const char *instance, int x, int y, int x_rel, int y_rel, int out_x, int out_y, int width, int height, int mods) {
    if (!status_child.click_events) {
        return;
    }

    child_click_events_initialize();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    if (name) {
        yyjson_mut_obj_add_str(doc, obj, "name", name);
    }

    if (instance) {
        yyjson_mut_obj_add_str(doc, obj, "instance", instance);
    }

    yyjson_mut_obj_add_int(doc, obj, "button", button);

    yyjson_mut_val *modifiers = yyjson_mut_arr(doc);
    if (mods & XCB_MOD_MASK_SHIFT) {
        yyjson_mut_arr_add_str(doc, modifiers, "Shift");
    }
    if (mods & XCB_MOD_MASK_CONTROL) {
        yyjson_mut_arr_add_str(doc, modifiers, "Control");
    }
    if (mods & XCB_MOD_MASK_1) {
        yyjson_mut_arr_add_str(doc, modifiers, "Mod1");
    }
    if (mods & XCB_MOD_MASK_2) {
        yyjson_mut_arr_add_str(doc, modifiers, "Mod2");
    }
    if (mods & XCB_MOD_MASK_3) {
        yyjson_mut_arr_add_str(doc, modifiers, "Mod3");
    }
    if (mods & XCB_MOD_MASK_4) {
        yyjson_mut_arr_add_str(doc, modifiers, "Mod4");
    }
    if (mods & XCB_MOD_MASK_5) {
        yyjson_mut_arr_add_str(doc, modifiers, "Mod5");
    }
    yyjson_mut_obj_add_val(doc, obj, "modifiers", modifiers);

    yyjson_mut_obj_add_int(doc, obj, "x", x);
    yyjson_mut_obj_add_int(doc, obj, "y", y);
    yyjson_mut_obj_add_int(doc, obj, "relative_x", x_rel);
    yyjson_mut_obj_add_int(doc, obj, "relative_y", y_rel);
    yyjson_mut_obj_add_int(doc, obj, "output_x", out_x);
    yyjson_mut_obj_add_int(doc, obj, "output_y", out_y);
    yyjson_mut_obj_add_int(doc, obj, "width", width);
    yyjson_mut_obj_add_int(doc, obj, "height", height);

    size_t size;
    char *output = yyjson_mut_write(doc, 0, &size);
    if (output != NULL) {
        ssize_t n = writeall(child_stdin, output, size);
        if (n != -1) {
            n = writeall(child_stdin, ",\n", 2);
        }
        free(output);

        if (n == -1) {
            status_child.click_events = false;
            kill_child();
            set_statusline_error("child_write_output failed");
            draw_bars(false);
        }
    }

    yyjson_mut_doc_free(doc);
}

static bool is_alive(i3bar_child *c) {
    return c->pid > 0;
}

/*
 * Returns true if the status child process is alive.
 *
 */
bool status_child_is_alive(void) {
    return is_alive(&status_child);
}

/*
 * Returns true if the workspace child process is alive.
 *
 */
bool ws_child_is_alive(void) {
    return is_alive(&ws_child);
}

/*
 * kill()s the child process (if any). Called when exit()ing.
 *
 */
void kill_children_at_exit(void) {
    DLOG_CHILDREN;
    cont_children();

    if (is_alive(&status_child)) {
        killpg(status_child.pid, SIGTERM);
    }
    if (is_alive(&ws_child)) {
        killpg(ws_child.pid, SIGTERM);
    }
}

static void cont_child(i3bar_child *c) {
    if (is_alive(c) && c->cont_signal > 0 && c->stopped) {
        c->stopped = false;
        killpg(c->pid, c->cont_signal);
    }
}

static void kill_and_wait(i3bar_child *c) {
    DLOG_CHILD(*c);
    if (!is_alive(c)) {
        return;
    }

    cont_child(c);
    killpg(c->pid, SIGTERM);
    int status;
    waitpid(c->pid, &status, 0);
    cleanup(c);
}

/*
 * kill()s the child process (if any) and closes and free()s the stdin- and
 * SIGCHLD-watchers
 *
 */
void kill_child(void) {
    kill_and_wait(&status_child);
}

/*
 * kill()s the workspace child process (if any) and closes and free()s the
 * stdin- and SIGCHLD-watchers.
 * Similar to kill_child.
 *
 */
void kill_ws_child(void) {
    kill_and_wait(&ws_child);
}

static void stop_child(i3bar_child *c) {
    if (c->stop_signal > 0 && !c->stopped) {
        c->stopped = true;
        killpg(c->pid, c->stop_signal);
    }
}

/*
 * Sends a SIGSTOP to the child process (if existent)
 *
 */
void stop_children(void) {
    DLOG_CHILDREN;
    stop_child(&status_child);
    stop_child(&ws_child);
}

/*
 * Sends a SIGCONT to the child process (if existent)
 *
 */
void cont_children(void) {
    DLOG_CHILDREN;

    cont_child(&status_child);
    cont_child(&ws_child);
}

/*
 * Whether or not the child want click events
 *
 */
bool child_want_click_events(void) {
    return status_child.click_events;
}
