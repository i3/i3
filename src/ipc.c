/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ipc.c: UNIX domain socket IPC (initialization, client handling, protocol).
 *
 */

#include "all.h"

#include <ev.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <yyjson.h>

char *current_socketpath = NULL;

TAILQ_HEAD(ipc_client_head, ipc_client) all_clients = TAILQ_HEAD_INITIALIZER(all_clients);

static void ipc_client_timeout(EV_P_ ev_timer *w, int revents);
static void ipc_socket_writeable_cb(EV_P_ ev_io *w, int revents);

static ev_tstamp kill_timeout = 10.0;

void ipc_set_kill_timeout(ev_tstamp new) {
    kill_timeout = new;
}

/*
 * Try to write the contents of the pending buffer to the client's subscription
 * socket. Will set, reset or clear the timeout and io write callbacks depending
 * on the result of the write operation.
 *
 */
static void ipc_push_pending(ipc_client *client) {
    const ssize_t result = writeall_nonblock(client->fd, client->buffer, client->buffer_size);
    if (result < 0) {
        return;
    }

    if ((size_t)result == client->buffer_size) {
        /* Everything was written successfully: clear the timer and stop the io
         * callback. */
        FREE(client->buffer);
        client->buffer_size = 0;
        if (client->timeout) {
            ev_timer_stop(main_loop, client->timeout);
            FREE(client->timeout);
        }
        ev_io_stop(main_loop, client->write_callback);
        return;
    }

    /* Otherwise, make sure that the io callback is enabled and create a new
     * timer if needed. */
    ev_io_start(main_loop, client->write_callback);

    if (!client->timeout) {
        ev_timer *timeout = scalloc(1, sizeof(struct ev_timer));
        ev_timer_init(timeout, ipc_client_timeout, kill_timeout, 0.);
        timeout->data = client;
        client->timeout = timeout;
        ev_set_priority(timeout, EV_MINPRI);
        ev_timer_start(main_loop, client->timeout);
    } else if (result > 0) {
        /* Keep the old timeout when nothing is written. Otherwise, we would
         * keep a dead connection by continuously renewing its timeouts. */
        ev_timer_stop(main_loop, client->timeout);
        ev_timer_set(client->timeout, kill_timeout, 0.0);
        ev_timer_start(main_loop, client->timeout);
    }
    if (result == 0) {
        return;
    }

    /* Shift the buffer to the left and reduce the allocated space. */
    client->buffer_size -= (size_t)result;
    memmove(client->buffer, client->buffer + result, client->buffer_size);
    client->buffer = srealloc(client->buffer, client->buffer_size);
}

/*
 * Given a message and a message type, create the corresponding header, merge it
 * with the message and append it to the given client's output buffer. Also,
 * send the message if the client's buffer was empty.
 *
 */
static void ipc_send_client_message(ipc_client *client, size_t size, const uint32_t message_type, const uint8_t *payload) {
    const i3_ipc_header_t header = {
        .magic = {'i', '3', '-', 'i', 'p', 'c'},
        .size = size,
        .type = message_type};
    const size_t header_size = sizeof(i3_ipc_header_t);
    const size_t message_size = header_size + size;

    const bool push_now = (client->buffer_size == 0);
    client->buffer = srealloc(client->buffer, client->buffer_size + message_size);
    memcpy(client->buffer + client->buffer_size, ((void *)&header), header_size);
    memcpy(client->buffer + client->buffer_size + header_size, payload, size);
    client->buffer_size += message_size;

    if (push_now) {
        ipc_push_pending(client);
    }
}

static void free_ipc_client(ipc_client *client, int exempt_fd) {
    if (client->fd != exempt_fd) {
        DLOG("Disconnecting client on fd %d\n", client->fd);
        close(client->fd);
    }

    ev_io_stop(main_loop, client->read_callback);
    FREE(client->read_callback);
    ev_io_stop(main_loop, client->write_callback);
    FREE(client->write_callback);
    if (client->timeout) {
        ev_timer_stop(main_loop, client->timeout);
        FREE(client->timeout);
    }

    free(client->buffer);

    for (int i = 0; i < client->num_events; i++) {
        free(client->events[i]);
    }
    free(client->events);
    TAILQ_REMOVE(&all_clients, client, clients);
    free(client);
}

/*
 * Sends the specified event to all IPC clients which are currently connected
 * and subscribed to this kind of event.
 *
 */
void ipc_send_event(const char *event, uint32_t message_type, const char *payload) {
    ipc_client *current;
    TAILQ_FOREACH (current, &all_clients, clients) {
        for (int i = 0; i < current->num_events; i++) {
            if (strcasecmp(current->events[i], event) == 0) {
                ipc_send_client_message(current, strlen(payload), message_type, (uint8_t *)payload);
                break;
            }
        }
    }
}

/*
 * For shutdown events, we send the reason for the shutdown.
 */
static void ipc_send_shutdown_event(const shutdown_reason_t reason) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    const char *change_str = (reason == SHUTDOWN_REASON_RESTART) ? "restart" : "exit";
    yyjson_mut_obj_add_str(doc, root, "change", change_str);

    size_t length;
    char *payload = json_write(doc, &length);
    ipc_send_event("shutdown", I3_IPC_EVENT_SHUTDOWN, payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Calls shutdown() on each socket and closes it. This function is to be called
 * when exiting or restarting only!
 *
 * exempt_fd is never closed. Set to -1 to close all fds.
 *
 */
void ipc_shutdown(shutdown_reason_t reason, int exempt_fd) {
    ipc_send_shutdown_event(reason);

    while (!TAILQ_EMPTY(&all_clients)) {
        ipc_client *current = TAILQ_FIRST(&all_clients);
        if (current->fd != exempt_fd) {
            shutdown(current->fd, SHUT_RDWR);
        }
        free_ipc_client(current, exempt_fd);
    }
}

/*
 * Executes the given command.
 *
 */
IPC_HANDLER(run_command) {
    /* To get a properly terminated buffer, we copy
     * message_size bytes out of the buffer */
    char *command = sstrndup((const char *)message, message_size);
    LOG("IPC: received: *%.4000s*\n", command);
    yyjson_mut_doc *doc = json_new();

    CommandResult *result = parse_command(command, doc, client);
    free(command);

    if (result->needs_tree_render) {
        tree_render();
    }

    command_result_free(result);

    size_t length;
    char *reply = json_write(doc, &length);
    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_COMMAND,
                            (const uint8_t *)reply);
    free(reply);
    yyjson_mut_doc_free(doc);
}

static yyjson_mut_val *dump_rect(yyjson_mut_doc *doc, Rect r) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, obj, "x", (int32_t)r.x);
    yyjson_mut_obj_add_int(doc, obj, "y", (int32_t)r.y);
    yyjson_mut_obj_add_int(doc, obj, "width", r.width);
    yyjson_mut_obj_add_int(doc, obj, "height", r.height);
    return obj;
}

static yyjson_mut_val *dump_gaps(yyjson_mut_doc *doc, gaps_t gaps) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, obj, "inner", gaps.inner);
    // TODO: the i3ipc Python modules recognize gaps, but only inner/outer
    // This is currently here to preserve compatibility with that
    yyjson_mut_obj_add_int(doc, obj, "outer", gaps.top);
    yyjson_mut_obj_add_int(doc, obj, "top", gaps.top);
    yyjson_mut_obj_add_int(doc, obj, "right", gaps.right);
    yyjson_mut_obj_add_int(doc, obj, "bottom", gaps.bottom);
    yyjson_mut_obj_add_int(doc, obj, "left", gaps.left);
    return obj;
}

static yyjson_mut_val *dump_event_state_mask(yyjson_mut_doc *doc, Binding *bind) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    for (int i = 0; i < 20; i++) {
        if (bind->event_state_mask & (1 << i)) {
            switch (1 << i) {
                case XCB_KEY_BUT_MASK_SHIFT:
                    yyjson_mut_arr_add_str(doc, arr, "shift");
                    break;
                case XCB_KEY_BUT_MASK_LOCK:
                    yyjson_mut_arr_add_str(doc, arr, "lock");
                    break;
                case XCB_KEY_BUT_MASK_CONTROL:
                    yyjson_mut_arr_add_str(doc, arr, "ctrl");
                    break;
                case XCB_KEY_BUT_MASK_MOD_1:
                    yyjson_mut_arr_add_str(doc, arr, "Mod1");
                    break;
                case XCB_KEY_BUT_MASK_MOD_2:
                    yyjson_mut_arr_add_str(doc, arr, "Mod2");
                    break;
                case XCB_KEY_BUT_MASK_MOD_3:
                    yyjson_mut_arr_add_str(doc, arr, "Mod3");
                    break;
                case XCB_KEY_BUT_MASK_MOD_4:
                    yyjson_mut_arr_add_str(doc, arr, "Mod4");
                    break;
                case XCB_KEY_BUT_MASK_MOD_5:
                    yyjson_mut_arr_add_str(doc, arr, "Mod5");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_1:
                    yyjson_mut_arr_add_str(doc, arr, "Button1");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_2:
                    yyjson_mut_arr_add_str(doc, arr, "Button2");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_3:
                    yyjson_mut_arr_add_str(doc, arr, "Button3");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_4:
                    yyjson_mut_arr_add_str(doc, arr, "Button4");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_5:
                    yyjson_mut_arr_add_str(doc, arr, "Button5");
                    break;
                case (I3_XKB_GROUP_MASK_1 << 16):
                    yyjson_mut_arr_add_str(doc, arr, "Group1");
                    break;
                case (I3_XKB_GROUP_MASK_2 << 16):
                    yyjson_mut_arr_add_str(doc, arr, "Group2");
                    break;
                case (I3_XKB_GROUP_MASK_3 << 16):
                    yyjson_mut_arr_add_str(doc, arr, "Group3");
                    break;
                case (I3_XKB_GROUP_MASK_4 << 16):
                    yyjson_mut_arr_add_str(doc, arr, "Group4");
                    break;
            }
        }
    }
    return arr;
}

static yyjson_mut_val *dump_binding(yyjson_mut_doc *doc, Binding *bind) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);

    yyjson_mut_obj_add_int(doc, obj, "input_code", bind->keycode);
    yyjson_mut_obj_add_str(doc, obj, "input_type",
                           bind->input_type == B_KEYBOARD ? "keyboard" : "mouse");

    if (bind->symbol == NULL) {
        yyjson_mut_obj_add_null(doc, obj, "symbol");
    } else {
        yyjson_mut_obj_add_str(doc, obj, "symbol", bind->symbol);
    }

    yyjson_mut_obj_add_str(doc, obj, "command", bind->command);

    // This key is only provided for compatibility, new programs should use
    // event_state_mask instead.
    yyjson_mut_obj_add_val(doc, obj, "mods", dump_event_state_mask(doc, bind));
    yyjson_mut_obj_add_val(doc, obj, "event_state_mask", dump_event_state_mask(doc, bind));

    return obj;
}

yyjson_mut_val *dump_node(yyjson_mut_doc *doc, Con *con, bool inplace_restart) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);

    yyjson_mut_obj_add_uint(doc, obj, "id", (uintptr_t)con);

    const char *type_str;
    switch (con->type) {
        case CT_ROOT:
            type_str = "root";
            break;
        case CT_OUTPUT:
            type_str = "output";
            break;
        case CT_CON:
            type_str = "con";
            break;
        case CT_FLOATING_CON:
            type_str = "floating_con";
            break;
        case CT_WORKSPACE:
            type_str = "workspace";
            break;
        case CT_DOCKAREA:
            type_str = "dockarea";
            break;
        default:
            type_str = "unknown";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "type", type_str);

    /* provided for backwards compatibility only. */
    const char *orientation_str;
    if (!con_is_split(con)) {
        orientation_str = "none";
    } else {
        orientation_str = (con_orientation(con) == HORIZ) ? "horizontal" : "vertical";
    }
    yyjson_mut_obj_add_str(doc, obj, "orientation", orientation_str);

    const char *scratchpad_str;
    switch (con->scratchpad_state) {
        case SCRATCHPAD_NONE:
            scratchpad_str = "none";
            break;
        case SCRATCHPAD_FRESH:
            scratchpad_str = "fresh";
            break;
        case SCRATCHPAD_CHANGED:
            scratchpad_str = "changed";
            break;
        default:
            scratchpad_str = "none";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "scratchpad_state", scratchpad_str);

    if (con->percent == 0.0) {
        yyjson_mut_obj_add_null(doc, obj, "percent");
    } else {
        yyjson_mut_obj_add_real(doc, obj, "percent", con->percent);
    }

    yyjson_mut_obj_add_bool(doc, obj, "urgent", con->urgent);

    yyjson_mut_val *marks_arr = yyjson_mut_arr(doc);
    mark_t *mark;
    TAILQ_FOREACH (mark, &(con->marks_head), marks) {
        yyjson_mut_arr_add_str(doc, marks_arr, mark->name);
    }
    yyjson_mut_obj_add_val(doc, obj, "marks", marks_arr);

    yyjson_mut_obj_add_bool(doc, obj, "focused", (con == focused));

    if (con->type != CT_ROOT && con->type != CT_OUTPUT) {
        yyjson_mut_obj_add_str(doc, obj, "output", con_get_output(con)->name);
    }

    const char *layout_str;
    switch (con->layout) {
        case L_DEFAULT:
            DLOG("About to dump layout=default, this is a bug in the code.\n");
            assert(false);
            layout_str = "default";
            break;
        case L_SPLITV:
            layout_str = "splitv";
            break;
        case L_SPLITH:
            layout_str = "splith";
            break;
        case L_STACKED:
            layout_str = "stacked";
            break;
        case L_TABBED:
            layout_str = "tabbed";
            break;
        case L_DOCKAREA:
            layout_str = "dockarea";
            break;
        case L_OUTPUT:
            layout_str = "output";
            break;
        default:
            layout_str = "unknown";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "layout", layout_str);

    const char *ws_layout_str;
    switch (con->workspace_layout) {
        case L_DEFAULT:
            ws_layout_str = "default";
            break;
        case L_STACKED:
            ws_layout_str = "stacked";
            break;
        case L_TABBED:
            ws_layout_str = "tabbed";
            break;
        default:
            DLOG("About to dump workspace_layout=%d (none of default/stacked/tabbed), this is a bug.\n", con->workspace_layout);
            assert(false);
            ws_layout_str = "default";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "workspace_layout", ws_layout_str);

    yyjson_mut_obj_add_str(doc, obj, "last_split_layout",
                           (con->layout == L_SPLITV) ? "splitv" : "splith");

    const char *border_str;
    switch (con->border_style) {
        case BS_NORMAL:
            border_str = "normal";
            break;
        case BS_NONE:
            border_str = "none";
            break;
        case BS_PIXEL:
            border_str = "pixel";
            break;
        default:
            border_str = "unknown";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "border", border_str);

    yyjson_mut_obj_add_int(doc, obj, "current_border_width", con->current_border_width);

    yyjson_mut_obj_add_val(doc, obj, "rect", dump_rect(doc, con->rect));

    if (con_draw_decoration_into_frame(con)) {
        Rect simulated_deco_rect = con->deco_rect;
        simulated_deco_rect.x = con->rect.x - con->parent->rect.x;
        simulated_deco_rect.y = con->rect.y - con->parent->rect.y;
        yyjson_mut_obj_add_val(doc, obj, "deco_rect", dump_rect(doc, simulated_deco_rect));
        yyjson_mut_obj_add_val(doc, obj, "actual_deco_rect", dump_rect(doc, con->deco_rect));
    } else {
        yyjson_mut_obj_add_val(doc, obj, "deco_rect", dump_rect(doc, con->deco_rect));
    }

    yyjson_mut_obj_add_val(doc, obj, "window_rect", dump_rect(doc, con->window_rect));
    yyjson_mut_obj_add_val(doc, obj, "geometry", dump_rect(doc, con->geometry));

    if (con->window && con->window->name) {
        yyjson_mut_obj_add_str(doc, obj, "name", i3string_as_utf8(con->window->name));
    } else if (con->name != NULL) {
        yyjson_mut_obj_add_str(doc, obj, "name", con->name);
    } else {
        yyjson_mut_obj_add_null(doc, obj, "name");
    }

    if (con->title_format != NULL) {
        yyjson_mut_obj_add_str(doc, obj, "title_format", con->title_format);
    }

    yyjson_mut_obj_add_int(doc, obj, "window_icon_padding", con->window_icon_padding);

    if (con->type == CT_WORKSPACE) {
        yyjson_mut_obj_add_int(doc, obj, "num", con->num);
        yyjson_mut_obj_add_val(doc, obj, "gaps", dump_gaps(doc, con->gaps));
    }

    if (con->window) {
        yyjson_mut_obj_add_uint(doc, obj, "window", con->window->id);
    } else {
        yyjson_mut_obj_add_null(doc, obj, "window");
    }

    const char *window_type_str = NULL;
    if (con->window) {
        if (con->window->window_type == A__NET_WM_WINDOW_TYPE_NORMAL) {
            window_type_str = "normal";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_DOCK) {
            window_type_str = "dock";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_DIALOG) {
            window_type_str = "dialog";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_UTILITY) {
            window_type_str = "utility";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_TOOLBAR) {
            window_type_str = "toolbar";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_SPLASH) {
            window_type_str = "splash";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_MENU) {
            window_type_str = "menu";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_DROPDOWN_MENU) {
            window_type_str = "dropdown_menu";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_POPUP_MENU) {
            window_type_str = "popup_menu";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_TOOLTIP) {
            window_type_str = "tooltip";
        } else if (con->window->window_type == A__NET_WM_WINDOW_TYPE_NOTIFICATION) {
            window_type_str = "notification";
        } else {
            window_type_str = "unknown";
        }
        yyjson_mut_obj_add_str(doc, obj, "window_type", window_type_str);
    } else {
        yyjson_mut_obj_add_null(doc, obj, "window_type");
    }

    if (con->window && !inplace_restart) {
        /* Window properties are useless to preserve when restarting because
         * they will be queried again anyway. However, for i3-save-tree(1),
         * they are very useful and save i3-save-tree dealing with X11. */
        yyjson_mut_val *props = yyjson_mut_obj(doc);

        if (con->window->class_class != NULL) {
            yyjson_mut_obj_add_str(doc, props, "class", con->window->class_class);
        }
        if (con->window->class_instance != NULL) {
            yyjson_mut_obj_add_str(doc, props, "instance", con->window->class_instance);
        }
        if (con->window->role != NULL) {
            yyjson_mut_obj_add_str(doc, props, "window_role", con->window->role);
        }
        if (con->window->machine != NULL) {
            yyjson_mut_obj_add_str(doc, props, "machine", con->window->machine);
        }
        if (con->window->name != NULL) {
            yyjson_mut_obj_add_str(doc, props, "title", i3string_as_utf8(con->window->name));
        }

        if (con->window->transient_for == XCB_NONE) {
            yyjson_mut_obj_add_null(doc, props, "transient_for");
        } else {
            yyjson_mut_obj_add_uint(doc, props, "transient_for", con->window->transient_for);
        }

        yyjson_mut_obj_add_val(doc, obj, "window_properties", props);
    }

    yyjson_mut_val *nodes_arr = yyjson_mut_arr(doc);
    Con *node;
    if (con->type != CT_DOCKAREA || !inplace_restart) {
        TAILQ_FOREACH (node, &(con->nodes_head), nodes) {
            yyjson_mut_arr_add_val(nodes_arr, dump_node(doc, node, inplace_restart));
        }
    }
    yyjson_mut_obj_add_val(doc, obj, "nodes", nodes_arr);

    yyjson_mut_val *floating_arr = yyjson_mut_arr(doc);
    TAILQ_FOREACH (node, &(con->floating_head), floating_windows) {
        yyjson_mut_arr_add_val(floating_arr, dump_node(doc, node, inplace_restart));
    }
    yyjson_mut_obj_add_val(doc, obj, "floating_nodes", floating_arr);

    yyjson_mut_val *focus_arr = yyjson_mut_arr(doc);
    TAILQ_FOREACH (node, &(con->focus_head), focused) {
        yyjson_mut_arr_add_uint(doc, focus_arr, (uintptr_t)node);
    }
    yyjson_mut_obj_add_val(doc, obj, "focus", focus_arr);

    yyjson_mut_obj_add_int(doc, obj, "fullscreen_mode", con->fullscreen_mode);
    yyjson_mut_obj_add_bool(doc, obj, "sticky", con->sticky);

    const char *floating_str;
    switch (con->floating) {
        case FLOATING_AUTO_OFF:
            floating_str = "auto_off";
            break;
        case FLOATING_AUTO_ON:
            floating_str = "auto_on";
            break;
        case FLOATING_USER_OFF:
            floating_str = "user_off";
            break;
        case FLOATING_USER_ON:
            floating_str = "user_on";
            break;
        default:
            floating_str = "auto_off";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "floating", floating_str);

    yyjson_mut_val *swallows_arr = yyjson_mut_arr(doc);
    Match *match;
    TAILQ_FOREACH (match, &(con->swallow_head), matches) {
        /* We will generate a new restart_mode match specification after this
         * loop, so skip this one. */
        if (match->restart_mode) {
            continue;
        }
        yyjson_mut_val *swallow_obj = yyjson_mut_obj(doc);
        if (match->dock != M_DONTCHECK) {
            yyjson_mut_obj_add_int(doc, swallow_obj, "dock", match->dock);
            yyjson_mut_obj_add_int(doc, swallow_obj, "insert_where", match->insert_where);
        }

        if (match->class != NULL) {
            yyjson_mut_obj_add_str(doc, swallow_obj, "class", match->class->pattern);
        }
        if (match->instance != NULL) {
            yyjson_mut_obj_add_str(doc, swallow_obj, "instance", match->instance->pattern);
        }
        if (match->window_role != NULL) {
            yyjson_mut_obj_add_str(doc, swallow_obj, "window_role", match->window_role->pattern);
        }
        if (match->title != NULL) {
            yyjson_mut_obj_add_str(doc, swallow_obj, "title", match->title->pattern);
        }
        if (match->machine != NULL) {
            yyjson_mut_obj_add_str(doc, swallow_obj, "machine", match->machine->pattern);
        }

        yyjson_mut_arr_add_val(swallows_arr, swallow_obj);
    }

    if (inplace_restart) {
        if (con->window != NULL) {
            yyjson_mut_val *swallow_obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_uint(doc, swallow_obj, "id", con->window->id);
            yyjson_mut_obj_add_bool(doc, swallow_obj, "restart_mode", true);
            yyjson_mut_arr_add_val(swallows_arr, swallow_obj);
        }
    }
    yyjson_mut_obj_add_val(doc, obj, "swallows", swallows_arr);

    if (inplace_restart && con->window != NULL) {
        yyjson_mut_obj_add_int(doc, obj, "depth", con->depth);
    }

    if (inplace_restart && con->type == CT_ROOT && previous_workspace_name) {
        yyjson_mut_obj_add_str(doc, obj, "previous_workspace_name", previous_workspace_name);
    }

    return obj;
}

static void dump_bar_bindings(yyjson_mut_doc *doc, yyjson_mut_val *obj, Barconfig *config) {
    if (TAILQ_EMPTY(&(config->bar_bindings))) {
        return;
    }

    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    struct Barbinding *current;
    TAILQ_FOREACH (current, &(config->bar_bindings), bindings) {
        yyjson_mut_val *binding_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_int(doc, binding_obj, "input_code", current->input_code);
        yyjson_mut_obj_add_str(doc, binding_obj, "command", current->command);
        yyjson_mut_obj_add_bool(doc, binding_obj, "release", current->release == B_UPON_KEYRELEASE);
        yyjson_mut_arr_add_val(arr, binding_obj);
    }

    yyjson_mut_obj_add_val(doc, obj, "bindings", arr);
}

static char *canonicalize_output_name(char *name) {
    /* Do not canonicalize special output names. */
    if (strcasecmp(name, "primary") == 0 || strcasecmp(name, "nonprimary") == 0) {
        return name;
    }
    Output *output = get_output_by_name(name, false);
    return output ? output_primary_name(output) : name;
}

static yyjson_mut_val *dump_bar_config(yyjson_mut_doc *doc, Barconfig *config) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);

    yyjson_mut_obj_add_str(doc, obj, "id", config->id);

    if (config->num_outputs > 0) {
        yyjson_mut_val *outputs_arr = yyjson_mut_arr(doc);
        for (int c = 0; c < config->num_outputs; c++) {
            /* Convert monitor names (RandR ≥ 1.5) or output names
             * (RandR < 1.5) into monitor names. This way, existing
             * configs which use output names transparently keep
             * working. */
            yyjson_mut_arr_add_str(doc, outputs_arr, canonicalize_output_name(config->outputs[c]));
        }
        yyjson_mut_obj_add_val(doc, obj, "outputs", outputs_arr);
    }

    if (!TAILQ_EMPTY(&(config->tray_outputs))) {
        yyjson_mut_val *tray_arr = yyjson_mut_arr(doc);
        struct tray_output_t *tray_output;
        TAILQ_FOREACH (tray_output, &(config->tray_outputs), tray_outputs) {
            yyjson_mut_arr_add_str(doc, tray_arr, canonicalize_output_name(tray_output->output));
        }
        yyjson_mut_obj_add_val(doc, obj, "tray_outputs", tray_arr);
    }

    yyjson_mut_obj_add_int(doc, obj, "tray_padding", config->tray_padding);

    if (config->socket_path) {
        yyjson_mut_obj_add_str(doc, obj, "socket_path", config->socket_path);
    }

    const char *mode_str;
    switch (config->mode) {
        case M_HIDE:
            mode_str = "hide";
            break;
        case M_INVISIBLE:
            mode_str = "invisible";
            break;
        case M_DOCK:
        default:
            mode_str = "dock";
            break;
    }
    yyjson_mut_obj_add_str(doc, obj, "mode", mode_str);

    yyjson_mut_obj_add_str(doc, obj, "hidden_state",
                           (config->hidden_state == S_SHOW) ? "show" : "hide");

    yyjson_mut_obj_add_int(doc, obj, "modifier", config->modifier);

    dump_bar_bindings(doc, obj, config);

    yyjson_mut_obj_add_str(doc, obj, "position",
                           (config->position == P_BOTTOM) ? "bottom" : "top");

    if (config->status_command) {
        yyjson_mut_obj_add_str(doc, obj, "status_command", config->status_command);
    }
    if (config->workspace_command) {
        yyjson_mut_obj_add_str(doc, obj, "workspace_command", config->workspace_command);
    }
    if (config->font) {
        yyjson_mut_obj_add_str(doc, obj, "font", config->font);
    }

    if (config->bar_height) {
        yyjson_mut_obj_add_int(doc, obj, "bar_height", config->bar_height);
    }

    yyjson_mut_obj_add_val(doc, obj, "padding", dump_rect(doc, config->padding));

    if (config->separator_symbol) {
        yyjson_mut_obj_add_str(doc, obj, "separator_symbol", config->separator_symbol);
    }

    yyjson_mut_obj_add_bool(doc, obj, "workspace_buttons", !config->hide_workspace_buttons);
    yyjson_mut_obj_add_int(doc, obj, "workspace_min_width", config->workspace_min_width);
    yyjson_mut_obj_add_bool(doc, obj, "strip_workspace_numbers", config->strip_workspace_numbers);
    yyjson_mut_obj_add_bool(doc, obj, "strip_workspace_name", config->strip_workspace_name);
    yyjson_mut_obj_add_bool(doc, obj, "binding_mode_indicator", !config->hide_binding_mode_indicator);
    yyjson_mut_obj_add_bool(doc, obj, "verbose", config->verbose);

    yyjson_mut_val *colors = yyjson_mut_obj(doc);

#define ADD_COLOR_IF_SET(name)                                         \
    do {                                                               \
        if (config->colors.name) {                                     \
            yyjson_mut_obj_add_str(doc, colors, #name, config->colors.name); \
        }                                                              \
    } while (0)

    ADD_COLOR_IF_SET(background);
    ADD_COLOR_IF_SET(statusline);
    ADD_COLOR_IF_SET(separator);
    ADD_COLOR_IF_SET(focused_background);
    ADD_COLOR_IF_SET(focused_statusline);
    ADD_COLOR_IF_SET(focused_separator);
    ADD_COLOR_IF_SET(focused_workspace_border);
    ADD_COLOR_IF_SET(focused_workspace_bg);
    ADD_COLOR_IF_SET(focused_workspace_text);
    ADD_COLOR_IF_SET(active_workspace_border);
    ADD_COLOR_IF_SET(active_workspace_bg);
    ADD_COLOR_IF_SET(active_workspace_text);
    ADD_COLOR_IF_SET(inactive_workspace_border);
    ADD_COLOR_IF_SET(inactive_workspace_bg);
    ADD_COLOR_IF_SET(inactive_workspace_text);
    ADD_COLOR_IF_SET(urgent_workspace_border);
    ADD_COLOR_IF_SET(urgent_workspace_bg);
    ADD_COLOR_IF_SET(urgent_workspace_text);
    ADD_COLOR_IF_SET(binding_mode_border);
    ADD_COLOR_IF_SET(binding_mode_bg);
    ADD_COLOR_IF_SET(binding_mode_text);

#undef ADD_COLOR_IF_SET

    yyjson_mut_obj_add_val(doc, obj, "colors", colors);

    return obj;
}

IPC_HANDLER(tree) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *root = dump_node(doc, croot, false);
    yyjson_mut_doc_set_root(doc, root);

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_TREE, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Formats the reply message for a GET_WORKSPACES request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_workspaces) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);

    Con *focused_ws = con_get_workspace(focused);

    Con *output;
    TAILQ_FOREACH (output, &(croot->nodes_head), nodes) {
        if (con_is_internal(output)) {
            continue;
        }
        Con *ws;
        TAILQ_FOREACH (ws, &(output_get_content(output)->nodes_head), nodes) {
            assert(ws->type == CT_WORKSPACE);
            yyjson_mut_val *ws_obj = yyjson_mut_obj(doc);

            yyjson_mut_obj_add_uint(doc, ws_obj, "id", (uintptr_t)ws);
            yyjson_mut_obj_add_int(doc, ws_obj, "num", ws->num);
            yyjson_mut_obj_add_str(doc, ws_obj, "name", ws->name);
            yyjson_mut_obj_add_bool(doc, ws_obj, "visible", workspace_is_visible(ws));
            yyjson_mut_obj_add_bool(doc, ws_obj, "focused", ws == focused_ws);
            yyjson_mut_obj_add_val(doc, ws_obj, "rect", dump_rect(doc, ws->rect));
            yyjson_mut_obj_add_str(doc, ws_obj, "output", output->name);
            yyjson_mut_obj_add_bool(doc, ws_obj, "urgent", ws->urgent);

            yyjson_mut_arr_add_val(arr, ws_obj);
        }
    }

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_WORKSPACES, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Formats the reply message for a GET_OUTPUTS request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_outputs) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);

    Output *output;
    TAILQ_FOREACH (output, &outputs, outputs) {
        yyjson_mut_val *output_obj = yyjson_mut_obj(doc);

        yyjson_mut_obj_add_str(doc, output_obj, "name", output_primary_name(output));
        yyjson_mut_obj_add_bool(doc, output_obj, "active", output->active);
        yyjson_mut_obj_add_bool(doc, output_obj, "primary", output->primary);
        yyjson_mut_obj_add_val(doc, output_obj, "rect", dump_rect(doc, output->rect));

        Con *ws = NULL;
        if (output->con && (ws = con_get_fullscreen_con(output->con, CF_OUTPUT))) {
            yyjson_mut_obj_add_str(doc, output_obj, "current_workspace", ws->name);
        } else {
            yyjson_mut_obj_add_null(doc, output_obj, "current_workspace");
        }

        yyjson_mut_arr_add_val(arr, output_obj);
    }

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_OUTPUTS, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Formats the reply message for a GET_MARKS request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_marks) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);

    Con *con;
    TAILQ_FOREACH (con, &all_cons, all_cons) {
        mark_t *mark;
        TAILQ_FOREACH (mark, &(con->marks_head), marks) {
            yyjson_mut_arr_add_str(doc, arr, mark->name);
        }
    }

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_MARKS, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Returns the version of i3
 *
 */
IPC_HANDLER(get_version) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    yyjson_mut_obj_add_int(doc, obj, "major", MAJOR_VERSION);
    yyjson_mut_obj_add_int(doc, obj, "minor", MINOR_VERSION);
    yyjson_mut_obj_add_int(doc, obj, "patch", PATCH_VERSION);
    yyjson_mut_obj_add_str(doc, obj, "human_readable", i3_version);
    yyjson_mut_obj_add_str(doc, obj, "loaded_config_file_name", current_configpath);

    yyjson_mut_val *included_arr = yyjson_mut_arr(doc);
    IncludedFile *file;
    TAILQ_FOREACH (file, &included_files, files) {
        if (file == TAILQ_FIRST(&included_files)) {
            /* Skip the first file, which is current_configpath. */
            continue;
        }
        yyjson_mut_arr_add_str(doc, included_arr, file->path);
    }
    yyjson_mut_obj_add_val(doc, obj, "included_config_file_names", included_arr);

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_VERSION, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Formats the reply message for a GET_BAR_CONFIG request and sends it to the
 * client.
 *
 */
IPC_HANDLER(get_bar_config) {
    yyjson_mut_doc *doc = json_new();

    /* If no ID was passed, we return a JSON array with all IDs */
    if (message_size == 0) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        yyjson_mut_doc_set_root(doc, arr);

        Barconfig *current;
        TAILQ_FOREACH (current, &barconfigs, configs) {
            yyjson_mut_arr_add_str(doc, arr, current->id);
        }

        size_t length;
        char *payload = json_write(doc, &length);

        ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_BAR_CONFIG, (const uint8_t *)payload);
        free(payload);
        yyjson_mut_doc_free(doc);
        return;
    }

    /* To get a properly terminated buffer, we copy
     * message_size bytes out of the buffer */
    char *bar_id = NULL;
    sasprintf(&bar_id, "%.*s", message_size, message);
    LOG("IPC: looking for config for bar ID \"%s\"\n", bar_id);
    Barconfig *current, *config = NULL;
    TAILQ_FOREACH (current, &barconfigs, configs) {
        if (strcmp(current->id, bar_id) != 0) {
            continue;
        }

        config = current;
        break;
    }
    free(bar_id);

    if (!config) {
        /* If we did not find a config for the given ID, the reply will contain
         * a null 'id' field. */
        yyjson_mut_val *obj = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, obj);
        yyjson_mut_obj_add_null(doc, obj, "id");
    } else {
        yyjson_mut_val *bar_obj = dump_bar_config(doc, config);
        yyjson_mut_doc_set_root(doc, bar_obj);
    }

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_BAR_CONFIG, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Returns a list of configured binding modes
 *
 */
IPC_HANDLER(get_binding_modes) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);

    struct Mode *mode;
    SLIST_FOREACH (mode, &modes, modes) {
        yyjson_mut_arr_add_str(doc, arr, mode->name);
    }

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_BINDING_MODES, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Subscribes this connection to the event types which were given as a JSON
 * serialized array in the payload field of the message.
 *
 */
IPC_HANDLER(subscribe) {
    yyjson_doc *req_doc = yyjson_read((const char *)message, message_size, 0);
    if (!req_doc) {
        ELOG("YYJSON parse error for subscribe\n");
        const char *reply = "{\"success\":false}";
        ipc_send_client_message(client, strlen(reply), I3_IPC_REPLY_TYPE_SUBSCRIBE, (const uint8_t *)reply);
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(req_doc);
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(req_doc);
        const char *reply = "{\"success\":false}";
        ipc_send_client_message(client, strlen(reply), I3_IPC_REPLY_TYPE_SUBSCRIBE, (const uint8_t *)reply);
        return;
    }

    size_t idx, max;
    yyjson_val *val;
    yyjson_arr_foreach(root, idx, max, val) {
        if (!yyjson_is_str(val)) {
            continue;
        }
        const char *event_str = yyjson_get_str(val);
        size_t event_len = yyjson_get_len(val);

        DLOG("should add subscription to client %p, sub %.*s\n", client, (int)event_len, event_str);
        int event = client->num_events;

        client->num_events++;
        client->events = srealloc(client->events, client->num_events * sizeof(char *));
        client->events[event] = scalloc(event_len + 1, 1);
        memcpy(client->events[event], event_str, event_len);
    }

    yyjson_doc_free(req_doc);

    DLOG("client is now subscribed to:\n");
    for (int i = 0; i < client->num_events; i++) {
        DLOG("event %s\n", client->events[i]);
    }
    DLOG("(done)\n");

    const char *reply = "{\"success\":true}";
    ipc_send_client_message(client, strlen(reply), I3_IPC_REPLY_TYPE_SUBSCRIBE, (const uint8_t *)reply);

    if (client->first_tick_sent) {
        return;
    }

    bool is_tick = false;
    for (int i = 0; i < client->num_events; i++) {
        if (strcmp(client->events[i], "tick") == 0) {
            is_tick = true;
            break;
        }
    }
    if (!is_tick) {
        return;
    }

    client->first_tick_sent = true;
    const char *tick_payload = "{\"first\":true,\"payload\":\"\"}";
    ipc_send_client_message(client, strlen(tick_payload), I3_IPC_EVENT_TICK, (const uint8_t *)tick_payload);
}

/*
 * Returns the raw last loaded i3 configuration file contents.
 */
IPC_HANDLER(get_config) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    IncludedFile *file = TAILQ_FIRST(&included_files);
    yyjson_mut_obj_add_str(doc, obj, "config", file->raw_contents);

    yyjson_mut_val *included_arr = yyjson_mut_arr(doc);
    TAILQ_FOREACH (file, &included_files, files) {
        yyjson_mut_val *file_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, file_obj, "path", file->path);
        yyjson_mut_obj_add_str(doc, file_obj, "raw_contents", file->raw_contents);
        yyjson_mut_obj_add_str(doc, file_obj, "variable_replaced_contents", file->variable_replaced_contents);
        yyjson_mut_arr_add_val(included_arr, file_obj);
    }
    yyjson_mut_obj_add_val(doc, obj, "included_configs", included_arr);

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_CONFIG, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Sends the tick event from the message payload to subscribers. Establishes a
 * synchronization point in event-related tests.
 */
IPC_HANDLER(send_tick) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    yyjson_mut_obj_add_bool(doc, obj, "first", false);
    yyjson_mut_obj_add_strn(doc, obj, "payload", (const char *)message, message_size);

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_event("tick", I3_IPC_EVENT_TICK, payload);
    free(payload);
    yyjson_mut_doc_free(doc);

    const char *reply = "{\"success\":true}";
    ipc_send_client_message(client, strlen(reply), I3_IPC_REPLY_TYPE_TICK, (const uint8_t *)reply);
    DLOG("Sent tick event\n");
}

IPC_HANDLER(sync) {
    yyjson_doc *req_doc = yyjson_read((const char *)message, message_size, 0);
    if (!req_doc) {
        ELOG("YYJSON parse error for sync\n");
        const char *reply = "{\"success\":false}";
        ipc_send_client_message(client, strlen(reply), I3_IPC_REPLY_TYPE_SYNC, (const uint8_t *)reply);
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(req_doc);
    uint32_t rnd = 0;
    xcb_window_t window = 0;

    if (yyjson_is_obj(root)) {
        yyjson_val *rnd_val = yyjson_obj_get(root, "rnd");
        if (rnd_val && yyjson_is_int(rnd_val)) {
            rnd = yyjson_get_int(rnd_val);
        }
        yyjson_val *window_val = yyjson_obj_get(root, "window");
        if (window_val && yyjson_is_int(window_val)) {
            window = (xcb_window_t)yyjson_get_int(window_val);
        }
    }

    yyjson_doc_free(req_doc);

    DLOG("received IPC sync request (rnd = %d, window = 0x%08x)\n", rnd, window);
    sync_respond(window, rnd);
    const char *reply = "{\"success\":true}";
    ipc_send_client_message(client, strlen(reply), I3_IPC_REPLY_TYPE_SYNC, (const uint8_t *)reply);
}

IPC_HANDLER(get_binding_state) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    yyjson_mut_obj_add_str(doc, obj, "name", current_binding_mode);

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_client_message(client, length, I3_IPC_REPLY_TYPE_GET_BINDING_STATE, (const uint8_t *)payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/* The index of each callback function corresponds to the numeric
 * value of the message type (see include/i3/ipc.h) */
static handler_t handlers[13] = {
    handle_run_command,
    handle_get_workspaces,
    handle_subscribe,
    handle_get_outputs,
    handle_tree,
    handle_get_marks,
    handle_get_bar_config,
    handle_get_version,
    handle_get_binding_modes,
    handle_get_config,
    handle_send_tick,
    handle_sync,
    handle_get_binding_state,
};

/*
 * Handler for activity on a client connection, receives a message from a
 * client.
 *
 */
static void ipc_receive_message(EV_P_ struct ev_io *w, int revents) {
    uint32_t message_type;
    uint32_t message_length;
    uint8_t *message = NULL;
    ipc_client *client = (ipc_client *)w->data;
    assert(client->fd == w->fd);

    const int ret = ipc_recv_message(client->fd, &message_type, &message_length, &message);
    /* EOF or other error */
    if (ret < 0) {
        /* Was this a spurious read? See ev(3) */
        if (ret == -1 && errno == EAGAIN) {
            free(message);
            return;
        }

        /* If not, there was some kind of error. We don't bother and
         * simply close the connection. */
        free_ipc_client(client, -1);
        free(message);
        return;
    }

    if (message_type >= (sizeof(handlers) / sizeof(handler_t))) {
        DLOG("Unhandled message type: %d\n", message_type);
    } else {
        handler_t handler = handlers[message_type];
        handler(client, message, 0, message_length, message_type);
    }

    free(message);
}

static void ipc_client_timeout(EV_P_ ev_timer *w, int revents) {
    /* No need to be polite and check the queue, we can just free_ipc_client()
     * since we have kill_timeout being too high. */
    ipc_client *client = (ipc_client *)w->data;

    ELOG("IPC client with pid %d on fd %d timed out, killing\n", client->fd, client->fd);
    free_ipc_client(client, -1);
}

static void ipc_socket_writeable_cb(EV_P_ ev_io *w, int revents) {
    ipc_client *client = (ipc_client *)w->data;
    assert(client->fd == w->fd);

    ipc_push_pending(client);
}

/*
 * Handler for activity on the listening socket, meaning that a new client
 * has just connected and we should accept() him. Sets up the event handler
 * for activity on the new connection and inserts the file descriptor into
 * the list of clients.
 *
 */
void ipc_new_client(EV_P_ struct ev_io *w, int revents) {
    struct sockaddr_un peer;
    socklen_t len = sizeof(struct sockaddr_un);
    const int fd = accept(w->fd, (struct sockaddr *)&peer, &len);
    if (fd < 0) {
        if (errno != EINTR) {
            perror("accept()");
        }
        return;
    }

    (void)ipc_new_client_on_fd(EV_A_ fd);
}

ipc_client *ipc_new_client_on_fd(EV_P_ int fd) {
    /* Set non-blocking */
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        ELOG("Could not set O_NONBLOCK on fd %d\n", fd);
        close(fd);
        return NULL;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ELOG("Could not set O_NONBLOCK on fd %d\n", fd);
        close(fd);
        return NULL;
    }

    ipc_client *client = scalloc(1, sizeof(ipc_client));
    client->fd = fd;

    client->read_callback = scalloc(1, sizeof(struct ev_io));
    client->read_callback->data = client;
    ev_io_init(client->read_callback, ipc_receive_message, fd, EV_READ);
    ev_io_start(EV_A_ client->read_callback);

    client->write_callback = scalloc(1, sizeof(struct ev_io));
    client->write_callback->data = client;
    ev_io_init(client->write_callback, ipc_socket_writeable_cb, fd, EV_WRITE);

    DLOG("IPC: new client connected on fd %d\n", fd);

    TAILQ_INSERT_TAIL(&all_clients, client, clients);

    return client;
}

/*
 * For the workspace events we send, along with the usual "change" field, also
 * the workspace container in "current". For focus events, we send the
 * previously focused workspace in "old".
 */
void ipc_send_workspace_event(const char *change, Con *current, Con *old) {
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    yyjson_mut_obj_add_str(doc, obj, "change", change);

    if (current == NULL) {
        yyjson_mut_obj_add_null(doc, obj, "current");
    } else {
        yyjson_mut_obj_add_val(doc, obj, "current", dump_node(doc, current, false));
    }

    if (old == NULL) {
        yyjson_mut_obj_add_null(doc, obj, "old");
    } else {
        yyjson_mut_obj_add_val(doc, obj, "old", dump_node(doc, old, false));
    }

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, payload);

    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * For the window events we send, along the usual "change" field,
 * also the window container, in "container".
 */
void ipc_send_window_event(const char *property, Con *con) {
    DLOG("Issue IPC window %s event (con = %p, window = 0x%08x)\n",
         property, con, (con->window ? con->window->id : XCB_WINDOW_NONE));

    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    yyjson_mut_obj_add_str(doc, obj, "change", property);
    yyjson_mut_obj_add_val(doc, obj, "container", dump_node(doc, con, false));

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_event("window", I3_IPC_EVENT_WINDOW, payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * For the barconfig update events, we send the serialized barconfig.
 */
void ipc_send_barconfig_update_event(Barconfig *barconfig) {
    DLOG("Issue barconfig_update event for id = %s\n", barconfig->id);
    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *bar_obj = dump_bar_config(doc, barconfig);
    yyjson_mut_doc_set_root(doc, bar_obj);

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_event("barconfig_update", I3_IPC_EVENT_BARCONFIG_UPDATE, payload);
    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * For the binding events, we send the serialized binding struct.
 */
void ipc_send_binding_event(const char *event_type, Binding *bind, const char *modename) {
    DLOG("Issue IPC binding %s event (sym = %s, code = %d)\n", event_type, bind->symbol, bind->keycode);

    yyjson_mut_doc *doc = json_new();
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, obj);

    yyjson_mut_obj_add_str(doc, obj, "change", event_type);

    if (modename == NULL) {
        yyjson_mut_obj_add_str(doc, obj, "mode", "default");
    } else {
        yyjson_mut_obj_add_str(doc, obj, "mode", modename);
    }

    yyjson_mut_obj_add_val(doc, obj, "binding", dump_binding(doc, bind));

    size_t length;
    char *payload = json_write(doc, &length);

    ipc_send_event("binding", I3_IPC_EVENT_BINDING, payload);

    free(payload);
    yyjson_mut_doc_free(doc);
}

/*
 * Sends a restart reply to the IPC client on the specified fd.
 */
void ipc_confirm_restart(ipc_client *client) {
    DLOG("ipc_confirm_restart(fd %d)\n", client->fd);
    static const char *reply = "[{\"success\":true}]";
    ipc_send_client_message(
        client, strlen(reply), I3_IPC_REPLY_TYPE_COMMAND,
        (const uint8_t *)reply);
    ipc_push_pending(client);
}