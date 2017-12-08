/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ipc.c: UNIX domain socket IPC (initialization, client handling, protocol).
 *
 */
#include "all.h"

#include "yajl_utils.h"

#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <libgen.h>
#include <ev.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>

char *current_socketpath = NULL;

TAILQ_HEAD(ipc_client_head, ipc_client)
all_clients = TAILQ_HEAD_INITIALIZER(all_clients);

/*
 * Puts the given socket file descriptor into non-blocking mode or dies if
 * setting O_NONBLOCK failed. Non-blocking sockets are a good idea for our
 * IPC model because we should by no means block the window manager.
 *
 */
static void set_nonblock(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) < 0)
        err(-1, "Could not set O_NONBLOCK");
}

/*
 * Sends the specified event to all IPC clients which are currently connected
 * and subscribed to this kind of event.
 *
 */
void ipc_send_event(const char *event, uint32_t message_type, const char *payload) {
    ipc_client *current;
    TAILQ_FOREACH(current, &all_clients, clients) {
        /* see if this client is interested in this event */
        bool interested = false;
        for (int i = 0; i < current->num_events; i++) {
            if (strcasecmp(current->events[i], event) != 0)
                continue;
            interested = true;
            break;
        }
        if (!interested)
            continue;

        ipc_send_message(current->fd, strlen(payload), message_type, (const uint8_t *)payload);
    }
}

/*
 * For shutdown events, we send the reason for the shutdown.
 */
static void ipc_send_shutdown_event(shutdown_reason_t reason) {
    yajl_gen gen = ygenalloc();
    y(map_open);

    ystr("change");

    if (reason == SHUTDOWN_REASON_RESTART) {
        ystr("restart");
    } else if (reason == SHUTDOWN_REASON_EXIT) {
        ystr("exit");
    }

    y(map_close);

    const unsigned char *payload;
    ylength length;

    y(get_buf, &payload, &length);
    ipc_send_event("shutdown", I3_IPC_EVENT_SHUTDOWN, (const char *)payload);

    y(free);
}

/*
 * Calls shutdown() on each socket and closes it. This function is to be called
 * when exiting or restarting only!
 *
 */
void ipc_shutdown(shutdown_reason_t reason) {
    ipc_send_shutdown_event(reason);

    ipc_client *current;
    while (!TAILQ_EMPTY(&all_clients)) {
        current = TAILQ_FIRST(&all_clients);
        shutdown(current->fd, SHUT_RDWR);
        close(current->fd);
        for (int i = 0; i < current->num_events; i++)
            free(current->events[i]);
        free(current->events);
        TAILQ_REMOVE(&all_clients, current, clients);
        free(current);
    }
}

/*
 * Executes the command and returns whether it could be successfully parsed
 * or not (at the moment, always returns true).
 *
 */
IPC_HANDLER(run_command) {
    /* To get a properly terminated buffer, we copy
     * message_size bytes out of the buffer */
    char *command = scalloc(message_size + 1, 1);
    strncpy(command, (const char *)message, message_size);
    LOG("IPC: received: *%s*\n", command);
    yajl_gen gen = yajl_gen_alloc(NULL);

    CommandResult *result = parse_command((const char *)command, gen);
    free(command);

    if (result->needs_tree_render)
        tree_render();

    command_result_free(result);

    const unsigned char *reply;
    ylength length;
    yajl_gen_get_buf(gen, &reply, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_COMMAND,
                     (const uint8_t *)reply);

    yajl_gen_free(gen);
}

static void dump_rect(yajl_gen gen, const char *name, Rect r) {
    ystr(name);
    y(map_open);
    ystr("x");
    y(integer, r.x);
    ystr("y");
    y(integer, r.y);
    ystr("width");
    y(integer, r.width);
    ystr("height");
    y(integer, r.height);
    y(map_close);
}

static void dump_event_state_mask(yajl_gen gen, Binding *bind) {
    y(array_open);
    for (int i = 0; i < 20; i++) {
        if (bind->event_state_mask & (1 << i)) {
            switch (1 << i) {
                case XCB_KEY_BUT_MASK_SHIFT:
                    ystr("shift");
                    break;
                case XCB_KEY_BUT_MASK_LOCK:
                    ystr("lock");
                    break;
                case XCB_KEY_BUT_MASK_CONTROL:
                    ystr("ctrl");
                    break;
                case XCB_KEY_BUT_MASK_MOD_1:
                    ystr("Mod1");
                    break;
                case XCB_KEY_BUT_MASK_MOD_2:
                    ystr("Mod2");
                    break;
                case XCB_KEY_BUT_MASK_MOD_3:
                    ystr("Mod3");
                    break;
                case XCB_KEY_BUT_MASK_MOD_4:
                    ystr("Mod4");
                    break;
                case XCB_KEY_BUT_MASK_MOD_5:
                    ystr("Mod5");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_1:
                    ystr("Button1");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_2:
                    ystr("Button2");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_3:
                    ystr("Button3");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_4:
                    ystr("Button4");
                    break;
                case XCB_KEY_BUT_MASK_BUTTON_5:
                    ystr("Button5");
                    break;
                case (I3_XKB_GROUP_MASK_1 << 16):
                    ystr("Group1");
                    break;
                case (I3_XKB_GROUP_MASK_2 << 16):
                    ystr("Group2");
                    break;
                case (I3_XKB_GROUP_MASK_3 << 16):
                    ystr("Group3");
                    break;
                case (I3_XKB_GROUP_MASK_4 << 16):
                    ystr("Group4");
                    break;
            }
        }
    }
    y(array_close);
}

static void dump_binding(yajl_gen gen, Binding *bind) {
    y(map_open);
    ystr("input_code");
    y(integer, bind->keycode);

    ystr("input_type");
    ystr((const char *)(bind->input_type == B_KEYBOARD ? "keyboard" : "mouse"));

    ystr("symbol");
    if (bind->symbol == NULL)
        y(null);
    else
        ystr(bind->symbol);

    ystr("command");
    ystr(bind->command);

    // This key is only provided for compatibility, new programs should use
    // event_state_mask instead.
    ystr("mods");
    dump_event_state_mask(gen, bind);

    ystr("event_state_mask");
    dump_event_state_mask(gen, bind);

    y(map_close);
}

void dump_node(yajl_gen gen, struct Con *con, bool inplace_restart) {
    y(map_open);
    ystr("id");
    y(integer, (uintptr_t)con);

    ystr("type");
    switch (con->type) {
        case CT_ROOT:
            ystr("root");
            break;
        case CT_OUTPUT:
            ystr("output");
            break;
        case CT_CON:
            ystr("con");
            break;
        case CT_FLOATING_CON:
            ystr("floating_con");
            break;
        case CT_WORKSPACE:
            ystr("workspace");
            break;
        case CT_DOCKAREA:
            ystr("dockarea");
            break;
        default:
            DLOG("About to dump unknown container type=%d. This is a bug.\n", con->type);
            assert(false);
            break;
    }

    /* provided for backwards compatibility only. */
    ystr("orientation");
    if (!con_is_split(con))
        ystr("none");
    else {
        if (con_orientation(con) == HORIZ)
            ystr("horizontal");
        else
            ystr("vertical");
    }

    ystr("scratchpad_state");
    switch (con->scratchpad_state) {
        case SCRATCHPAD_NONE:
            ystr("none");
            break;
        case SCRATCHPAD_FRESH:
            ystr("fresh");
            break;
        case SCRATCHPAD_CHANGED:
            ystr("changed");
            break;
    }

    ystr("percent");
    if (con->percent == 0.0)
        y(null);
    else
        y(double, con->percent);

    ystr("urgent");
    y(bool, con->urgent);

    if (!TAILQ_EMPTY(&(con->marks_head))) {
        ystr("marks");
        y(array_open);

        mark_t *mark;
        TAILQ_FOREACH(mark, &(con->marks_head), marks) {
            ystr(mark->name);
        }

        y(array_close);
    }

    ystr("focused");
    y(bool, (con == focused));

    if (con->type != CT_ROOT && con->type != CT_OUTPUT) {
        ystr("output");
        ystr(con_get_output(con)->name);
    }

    ystr("layout");
    switch (con->layout) {
        case L_DEFAULT:
            DLOG("About to dump layout=default, this is a bug in the code.\n");
            assert(false);
            break;
        case L_SPLITV:
            ystr("splitv");
            break;
        case L_SPLITH:
            ystr("splith");
            break;
        case L_STACKED:
            ystr("stacked");
            break;
        case L_TABBED:
            ystr("tabbed");
            break;
        case L_DOCKAREA:
            ystr("dockarea");
            break;
        case L_OUTPUT:
            ystr("output");
            break;
    }

    ystr("workspace_layout");
    switch (con->workspace_layout) {
        case L_DEFAULT:
            ystr("default");
            break;
        case L_STACKED:
            ystr("stacked");
            break;
        case L_TABBED:
            ystr("tabbed");
            break;
        default:
            DLOG("About to dump workspace_layout=%d (none of default/stacked/tabbed), this is a bug.\n", con->workspace_layout);
            assert(false);
            break;
    }

    ystr("last_split_layout");
    switch (con->layout) {
        case L_SPLITV:
            ystr("splitv");
            break;
        default:
            ystr("splith");
            break;
    }

    ystr("border");
    switch (con->border_style) {
        case BS_NORMAL:
            ystr("normal");
            break;
        case BS_NONE:
            ystr("none");
            break;
        case BS_PIXEL:
            ystr("pixel");
            break;
    }

    ystr("current_border_width");
    y(integer, con->current_border_width);

    dump_rect(gen, "rect", con->rect);
    dump_rect(gen, "deco_rect", con->deco_rect);
    dump_rect(gen, "window_rect", con->window_rect);
    dump_rect(gen, "geometry", con->geometry);

    ystr("name");
    if (con->window && con->window->name)
        ystr(i3string_as_utf8(con->window->name));
    else if (con->name != NULL)
        ystr(con->name);
    else
        y(null);

    if (con->title_format != NULL) {
        ystr("title_format");
        ystr(con->title_format);
    }

    if (con->type == CT_WORKSPACE) {
        ystr("num");
        y(integer, con->num);
    }

    ystr("window");
    if (con->window)
        y(integer, con->window->id);
    else
        y(null);

    if (con->window && !inplace_restart) {
        /* Window properties are useless to preserve when restarting because
         * they will be queried again anyway. However, for i3-save-tree(1),
         * they are very useful and save i3-save-tree dealing with X11. */
        ystr("window_properties");
        y(map_open);

#define DUMP_PROPERTY(key, prop_name)         \
    do {                                      \
        if (con->window->prop_name != NULL) { \
            ystr(key);                        \
            ystr(con->window->prop_name);     \
        }                                     \
    } while (0)

        DUMP_PROPERTY("class", class_class);
        DUMP_PROPERTY("instance", class_instance);
        DUMP_PROPERTY("window_role", role);

        if (con->window->name != NULL) {
            ystr("title");
            ystr(i3string_as_utf8(con->window->name));
        }

        ystr("transient_for");
        if (con->window->transient_for == XCB_NONE)
            y(null);
        else
            y(integer, con->window->transient_for);

        y(map_close);
    }

    ystr("nodes");
    y(array_open);
    Con *node;
    if (con->type != CT_DOCKAREA || !inplace_restart) {
        TAILQ_FOREACH(node, &(con->nodes_head), nodes) {
            dump_node(gen, node, inplace_restart);
        }
    }
    y(array_close);

    ystr("floating_nodes");
    y(array_open);
    TAILQ_FOREACH(node, &(con->floating_head), floating_windows) {
        dump_node(gen, node, inplace_restart);
    }
    y(array_close);

    ystr("focus");
    y(array_open);
    TAILQ_FOREACH(node, &(con->focus_head), focused) {
        y(integer, (uintptr_t)node);
    }
    y(array_close);

    ystr("fullscreen_mode");
    y(integer, con->fullscreen_mode);

    ystr("sticky");
    y(bool, con->sticky);

    ystr("floating");
    switch (con->floating) {
        case FLOATING_AUTO_OFF:
            ystr("auto_off");
            break;
        case FLOATING_AUTO_ON:
            ystr("auto_on");
            break;
        case FLOATING_USER_OFF:
            ystr("user_off");
            break;
        case FLOATING_USER_ON:
            ystr("user_on");
            break;
    }

    ystr("swallows");
    y(array_open);
    Match *match;
    TAILQ_FOREACH(match, &(con->swallow_head), matches) {
        /* We will generate a new restart_mode match specification after this
         * loop, so skip this one. */
        if (match->restart_mode)
            continue;
        y(map_open);
        if (match->dock != M_DONTCHECK) {
            ystr("dock");
            y(integer, match->dock);
            ystr("insert_where");
            y(integer, match->insert_where);
        }

#define DUMP_REGEX(re_name)                \
    do {                                   \
        if (match->re_name != NULL) {      \
            ystr(#re_name);                \
            ystr(match->re_name->pattern); \
        }                                  \
    } while (0)

        DUMP_REGEX(class);
        DUMP_REGEX(instance);
        DUMP_REGEX(window_role);
        DUMP_REGEX(title);

#undef DUMP_REGEX
        y(map_close);
    }

    if (inplace_restart) {
        if (con->window != NULL) {
            y(map_open);
            ystr("id");
            y(integer, con->window->id);
            ystr("restart_mode");
            y(bool, true);
            y(map_close);
        }
    }
    y(array_close);

    if (inplace_restart && con->window != NULL) {
        ystr("depth");
        y(integer, con->depth);
    }

    y(map_close);
}

static void dump_bar_bindings(yajl_gen gen, Barconfig *config) {
    if (TAILQ_EMPTY(&(config->bar_bindings)))
        return;

    ystr("bindings");
    y(array_open);

    struct Barbinding *current;
    TAILQ_FOREACH(current, &(config->bar_bindings), bindings) {
        y(map_open);

        ystr("input_code");
        y(integer, current->input_code);
        ystr("command");
        ystr(current->command);
        ystr("release");
        y(bool, current->release == B_UPON_KEYRELEASE);

        y(map_close);
    }

    y(array_close);
}

static char *canonicalize_output_name(char *name) {
    /* Do not canonicalize special output names. */
    if (strcasecmp(name, "primary") == 0) {
        return name;
    }
    Output *output = get_output_by_name(name, false);
    return output ? output_primary_name(output) : name;
}

static void dump_bar_config(yajl_gen gen, Barconfig *config) {
    y(map_open);

    ystr("id");
    ystr(config->id);

    if (config->num_outputs > 0) {
        ystr("outputs");
        y(array_open);
        for (int c = 0; c < config->num_outputs; c++) {
            /* Convert monitor names (RandR ≥ 1.5) or output names
             * (RandR < 1.5) into monitor names. This way, existing
             * configs which use output names transparently keep
             * working. */
            ystr(canonicalize_output_name(config->outputs[c]));
        }
        y(array_close);
    }

    if (!TAILQ_EMPTY(&(config->tray_outputs))) {
        ystr("tray_outputs");
        y(array_open);

        struct tray_output_t *tray_output;
        TAILQ_FOREACH(tray_output, &(config->tray_outputs), tray_outputs) {
            ystr(canonicalize_output_name(tray_output->output));
        }

        y(array_close);
    }

#define YSTR_IF_SET(name)       \
    do {                        \
        if (config->name) {     \
            ystr(#name);        \
            ystr(config->name); \
        }                       \
    } while (0)

    ystr("tray_padding");
    y(integer, config->tray_padding);

    YSTR_IF_SET(socket_path);

    ystr("mode");
    switch (config->mode) {
        case M_HIDE:
            ystr("hide");
            break;
        case M_INVISIBLE:
            ystr("invisible");
            break;
        case M_DOCK:
        default:
            ystr("dock");
            break;
    }

    ystr("hidden_state");
    switch (config->hidden_state) {
        case S_SHOW:
            ystr("show");
            break;
        case S_HIDE:
        default:
            ystr("hide");
            break;
    }

    ystr("modifier");
    switch (config->modifier) {
        case M_NONE:
            ystr("none");
            break;
        case M_CONTROL:
            ystr("ctrl");
            break;
        case M_SHIFT:
            ystr("shift");
            break;
        case M_MOD1:
            ystr("Mod1");
            break;
        case M_MOD2:
            ystr("Mod2");
            break;
        case M_MOD3:
            ystr("Mod3");
            break;
        case M_MOD5:
            ystr("Mod5");
            break;
        default:
            ystr("Mod4");
            break;
    }

    dump_bar_bindings(gen, config);

    ystr("position");
    if (config->position == P_BOTTOM)
        ystr("bottom");
    else
        ystr("top");

    YSTR_IF_SET(status_command);
    YSTR_IF_SET(font);

    if (config->separator_symbol) {
        ystr("separator_symbol");
        ystr(config->separator_symbol);
    }

    ystr("workspace_buttons");
    y(bool, !config->hide_workspace_buttons);

    ystr("strip_workspace_numbers");
    y(bool, config->strip_workspace_numbers);

    ystr("binding_mode_indicator");
    y(bool, !config->hide_binding_mode_indicator);

    ystr("verbose");
    y(bool, config->verbose);

#undef YSTR_IF_SET
#define YSTR_IF_SET(name)              \
    do {                               \
        if (config->colors.name) {     \
            ystr(#name);               \
            ystr(config->colors.name); \
        }                              \
    } while (0)

    ystr("colors");
    y(map_open);
    YSTR_IF_SET(background);
    YSTR_IF_SET(statusline);
    YSTR_IF_SET(separator);
    YSTR_IF_SET(focused_background);
    YSTR_IF_SET(focused_statusline);
    YSTR_IF_SET(focused_separator);
    YSTR_IF_SET(focused_workspace_border);
    YSTR_IF_SET(focused_workspace_bg);
    YSTR_IF_SET(focused_workspace_text);
    YSTR_IF_SET(active_workspace_border);
    YSTR_IF_SET(active_workspace_bg);
    YSTR_IF_SET(active_workspace_text);
    YSTR_IF_SET(inactive_workspace_border);
    YSTR_IF_SET(inactive_workspace_bg);
    YSTR_IF_SET(inactive_workspace_text);
    YSTR_IF_SET(urgent_workspace_border);
    YSTR_IF_SET(urgent_workspace_bg);
    YSTR_IF_SET(urgent_workspace_text);
    YSTR_IF_SET(binding_mode_border);
    YSTR_IF_SET(binding_mode_bg);
    YSTR_IF_SET(binding_mode_text);
    y(map_close);

    y(map_close);
#undef YSTR_IF_SET
}

IPC_HANDLER(tree) {
    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = ygenalloc();
    dump_node(gen, croot, false);
    setlocale(LC_NUMERIC, "");

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_TREE, payload);
    y(free);
}

/*
 * Formats the reply message for a GET_WORKSPACES request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_workspaces) {
    yajl_gen gen = ygenalloc();
    y(array_open);

    Con *focused_ws = con_get_workspace(focused);

    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        if (con_is_internal(output))
            continue;
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            assert(ws->type == CT_WORKSPACE);
            y(map_open);

            ystr("num");
            y(integer, ws->num);

            ystr("name");
            ystr(ws->name);

            ystr("visible");
            y(bool, workspace_is_visible(ws));

            ystr("focused");
            y(bool, ws == focused_ws);

            ystr("rect");
            y(map_open);
            ystr("x");
            y(integer, ws->rect.x);
            ystr("y");
            y(integer, ws->rect.y);
            ystr("width");
            y(integer, ws->rect.width);
            ystr("height");
            y(integer, ws->rect.height);
            y(map_close);

            ystr("output");
            ystr(output->name);

            ystr("urgent");
            y(bool, ws->urgent);

            y(map_close);
        }
    }

    y(array_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_WORKSPACES, payload);
    y(free);
}

/*
 * Formats the reply message for a GET_OUTPUTS request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_outputs) {
    yajl_gen gen = ygenalloc();
    y(array_open);

    Output *output;
    TAILQ_FOREACH(output, &outputs, outputs) {
        y(map_open);

        ystr("name");
        ystr(output_primary_name(output));

        ystr("active");
        y(bool, output->active);

        ystr("primary");
        y(bool, output->primary);

        ystr("rect");
        y(map_open);
        ystr("x");
        y(integer, output->rect.x);
        ystr("y");
        y(integer, output->rect.y);
        ystr("width");
        y(integer, output->rect.width);
        ystr("height");
        y(integer, output->rect.height);
        y(map_close);

        ystr("current_workspace");
        Con *ws = NULL;
        if (output->con && (ws = con_get_fullscreen_con(output->con, CF_OUTPUT)))
            ystr(ws->name);
        else
            y(null);

        y(map_close);
    }

    y(array_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_OUTPUTS, payload);
    y(free);
}

/*
 * Formats the reply message for a GET_MARKS request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_marks) {
    yajl_gen gen = ygenalloc();
    y(array_open);

    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons) {
        mark_t *mark;
        TAILQ_FOREACH(mark, &(con->marks_head), marks) {
            ystr(mark->name);
        }
    }

    y(array_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_MARKS, payload);
    y(free);
}

/*
 * Returns the version of i3
 *
 */
IPC_HANDLER(get_version) {
    yajl_gen gen = ygenalloc();
    y(map_open);

    ystr("major");
    y(integer, MAJOR_VERSION);

    ystr("minor");
    y(integer, MINOR_VERSION);

    ystr("patch");
    y(integer, PATCH_VERSION);

    ystr("human_readable");
    ystr(i3_version);

    ystr("loaded_config_file_name");
    ystr(current_configpath);

    y(map_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_VERSION, payload);
    y(free);
}

/*
 * Formats the reply message for a GET_BAR_CONFIG request and sends it to the
 * client.
 *
 */
IPC_HANDLER(get_bar_config) {
    yajl_gen gen = ygenalloc();

    /* If no ID was passed, we return a JSON array with all IDs */
    if (message_size == 0) {
        y(array_open);
        Barconfig *current;
        TAILQ_FOREACH(current, &barconfigs, configs) {
            ystr(current->id);
        }
        y(array_close);

        const unsigned char *payload;
        ylength length;
        y(get_buf, &payload, &length);

        ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_BAR_CONFIG, payload);
        y(free);
        return;
    }

    /* To get a properly terminated buffer, we copy
     * message_size bytes out of the buffer */
    char *bar_id = NULL;
    sasprintf(&bar_id, "%.*s", message_size, message);
    LOG("IPC: looking for config for bar ID \"%s\"\n", bar_id);
    Barconfig *current, *config = NULL;
    TAILQ_FOREACH(current, &barconfigs, configs) {
        if (strcmp(current->id, bar_id) != 0)
            continue;

        config = current;
        break;
    }
    free(bar_id);

    if (!config) {
        /* If we did not find a config for the given ID, the reply will contain
         * a null 'id' field. */
        y(map_open);

        ystr("id");
        y(null);

        y(map_close);
    } else {
        dump_bar_config(gen, config);
    }

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_BAR_CONFIG, payload);
    y(free);
}

/*
 * Returns a list of configured binding modes
 *
 */
IPC_HANDLER(get_binding_modes) {
    yajl_gen gen = ygenalloc();

    y(array_open);
    struct Mode *mode;
    SLIST_FOREACH(mode, &modes, modes) {
        ystr(mode->name);
    }
    y(array_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_BINDING_MODES, payload);
    y(free);
}

/*
 * Callback for the YAJL parser (will be called when a string is parsed).
 *
 */
static int add_subscription(void *extra, const unsigned char *s,
                            ylength len) {
    ipc_client *client = extra;

    DLOG("should add subscription to extra %p, sub %.*s\n", client, (int)len, s);
    int event = client->num_events;

    client->num_events++;
    client->events = srealloc(client->events, client->num_events * sizeof(char *));
    /* We copy the string because it is not null-terminated and strndup()
     * is missing on some BSD systems */
    client->events[event] = scalloc(len + 1, 1);
    memcpy(client->events[event], s, len);

    DLOG("client is now subscribed to:\n");
    for (int i = 0; i < client->num_events; i++) {
        DLOG("event %s\n", client->events[i]);
    }
    DLOG("(done)\n");

    return 1;
}

/*
 * Subscribes this connection to the event types which were given as a JSON
 * serialized array in the payload field of the message.
 *
 */
IPC_HANDLER(subscribe) {
    yajl_handle p;
    yajl_status stat;
    ipc_client *current, *client = NULL;

    /* Search the ipc_client structure for this connection */
    TAILQ_FOREACH(current, &all_clients, clients) {
        if (current->fd != fd)
            continue;

        client = current;
        break;
    }

    if (client == NULL) {
        ELOG("Could not find ipc_client data structure for fd %d\n", fd);
        return;
    }

    /* Setup the JSON parser */
    static yajl_callbacks callbacks = {
        .yajl_string = add_subscription,
    };

    p = yalloc(&callbacks, (void *)client);
    stat = yajl_parse(p, (const unsigned char *)message, message_size);
    if (stat != yajl_status_ok) {
        unsigned char *err;
        err = yajl_get_error(p, true, (const unsigned char *)message,
                             message_size);
        ELOG("YAJL parse error: %s\n", err);
        yajl_free_error(p, err);

        const char *reply = "{\"success\":false}";
        ipc_send_message(fd, strlen(reply), I3_IPC_REPLY_TYPE_SUBSCRIBE, (const uint8_t *)reply);
        yajl_free(p);
        return;
    }
    yajl_free(p);
    const char *reply = "{\"success\":true}";
    ipc_send_message(fd, strlen(reply), I3_IPC_REPLY_TYPE_SUBSCRIBE, (const uint8_t *)reply);

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
    const char *payload = "{\"first\":true,\"payload\":\"\"}";
    ipc_send_message(client->fd, strlen(payload), I3_IPC_EVENT_TICK, (const uint8_t *)payload);
}

/*
 * Returns the raw last loaded i3 configuration file contents.
 */
IPC_HANDLER(get_config) {
    yajl_gen gen = ygenalloc();

    y(map_open);

    ystr("config");
    ystr(current_config);

    y(map_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_message(fd, length, I3_IPC_REPLY_TYPE_CONFIG, payload);
    y(free);
}

/*
 * Sends the tick event from the message payload to subscribers. Establishes a
 * synchronization point in event-related tests.
 */
IPC_HANDLER(send_tick) {
    yajl_gen gen = ygenalloc();

    y(map_open);

    ystr("payload");
    yajl_gen_string(gen, (unsigned char *)message, message_size);

    y(map_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_event("tick", I3_IPC_EVENT_TICK, (const char *)payload);
    y(free);

    const char *reply = "{\"success\":true}";
    ipc_send_message(fd, strlen(reply), I3_IPC_REPLY_TYPE_TICK, (const uint8_t *)reply);
    DLOG("Sent tick event\n");
}

/* The index of each callback function corresponds to the numeric
 * value of the message type (see include/i3/ipc.h) */
handler_t handlers[11] = {
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
};

/*
 * Handler for activity on a client connection, receives a message from a
 * client.
 *
 * For now, the maximum message size is 2048. I’m not sure for what the
 * IPC interface will be used in the future, thus I’m not implementing a
 * mechanism for arbitrarily long messages, as it seems like overkill
 * at the moment.
 *
 */
static void ipc_receive_message(EV_P_ struct ev_io *w, int revents) {
    uint32_t message_type;
    uint32_t message_length;
    uint8_t *message = NULL;

    int ret = ipc_recv_message(w->fd, &message_type, &message_length, &message);
    /* EOF or other error */
    if (ret < 0) {
        /* Was this a spurious read? See ev(3) */
        if (ret == -1 && errno == EAGAIN) {
            FREE(message);
            return;
        }

        /* If not, there was some kind of error. We don’t bother
         * and close the connection */
        close(w->fd);

        /* Delete the client from the list of clients */
        ipc_client *current;
        TAILQ_FOREACH(current, &all_clients, clients) {
            if (current->fd != w->fd)
                continue;

            for (int i = 0; i < current->num_events; i++)
                free(current->events[i]);
            free(current->events);
            /* We can call TAILQ_REMOVE because we break out of the
             * TAILQ_FOREACH afterwards */
            TAILQ_REMOVE(&all_clients, current, clients);
            free(current);
            break;
        }

        ev_io_stop(EV_A_ w);
        free(w);
        FREE(message);

        DLOG("IPC: client disconnected\n");
        return;
    }

    if (message_type >= (sizeof(handlers) / sizeof(handler_t)))
        DLOG("Unhandled message type: %d\n", message_type);
    else {
        handler_t h = handlers[message_type];
        h(w->fd, message, 0, message_length, message_type);
    }

    FREE(message);
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
    int client;
    if ((client = accept(w->fd, (struct sockaddr *)&peer, &len)) < 0) {
        if (errno == EINTR)
            return;
        else
            perror("accept()");
        return;
    }

    /* Close this file descriptor on exec() */
    (void)fcntl(client, F_SETFD, FD_CLOEXEC);

    set_nonblock(client);

    struct ev_io *package = scalloc(1, sizeof(struct ev_io));
    ev_io_init(package, ipc_receive_message, client, EV_READ);
    ev_io_start(EV_A_ package);

    DLOG("IPC: new client connected on fd %d\n", w->fd);

    ipc_client *new = scalloc(1, sizeof(ipc_client));
    new->fd = client;

    TAILQ_INSERT_TAIL(&all_clients, new, clients);
}

/*
 * Creates the UNIX domain socket at the given path, sets it to non-blocking
 * mode, bind()s and listen()s on it.
 *
 */
int ipc_create_socket(const char *filename) {
    int sockfd;

    FREE(current_socketpath);

    char *resolved = resolve_tilde(filename);
    DLOG("Creating IPC-socket at %s\n", resolved);
    char *copy = sstrdup(resolved);
    const char *dir = dirname(copy);
    if (!path_exists(dir))
        mkdirp(dir, DEFAULT_DIR_MODE);
    free(copy);

    /* Unlink the unix domain socket before */
    unlink(resolved);

    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        free(resolved);
        return -1;
    }

    (void)fcntl(sockfd, F_SETFD, FD_CLOEXEC);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, resolved, sizeof(addr.sun_path) - 1);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind()");
        free(resolved);
        return -1;
    }

    set_nonblock(sockfd);

    if (listen(sockfd, 5) < 0) {
        perror("listen()");
        free(resolved);
        return -1;
    }

    current_socketpath = resolved;
    return sockfd;
}

/*
 * Generates a json workspace event. Returns a dynamically allocated yajl
 * generator. Free with yajl_gen_free().
 */
yajl_gen ipc_marshal_workspace_event(const char *change, Con *current, Con *old) {
    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = ygenalloc();

    y(map_open);

    ystr("change");
    ystr(change);

    ystr("current");
    if (current == NULL)
        y(null);
    else
        dump_node(gen, current, false);

    ystr("old");
    if (old == NULL)
        y(null);
    else
        dump_node(gen, old, false);

    y(map_close);

    setlocale(LC_NUMERIC, "");

    return gen;
}

/*
 * For the workspace events we send, along with the usual "change" field, also
 * the workspace container in "current". For focus events, we send the
 * previously focused workspace in "old".
 */
void ipc_send_workspace_event(const char *change, Con *current, Con *old) {
    yajl_gen gen = ipc_marshal_workspace_event(change, current, old);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, (const char *)payload);

    y(free);
}

/**
 * For the window events we send, along the usual "change" field,
 * also the window container, in "container".
 */
void ipc_send_window_event(const char *property, Con *con) {
    DLOG("Issue IPC window %s event (con = %p, window = 0x%08x)\n",
         property, con, (con->window ? con->window->id : XCB_WINDOW_NONE));

    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = ygenalloc();

    y(map_open);

    ystr("change");
    ystr(property);

    ystr("container");
    dump_node(gen, con, false);

    y(map_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_event("window", I3_IPC_EVENT_WINDOW, (const char *)payload);
    y(free);
    setlocale(LC_NUMERIC, "");
}

/**
 * For the barconfig update events, we send the serialized barconfig.
 */
void ipc_send_barconfig_update_event(Barconfig *barconfig) {
    DLOG("Issue barconfig_update event for id = %s\n", barconfig->id);
    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = ygenalloc();

    dump_bar_config(gen, barconfig);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_event("barconfig_update", I3_IPC_EVENT_BARCONFIG_UPDATE, (const char *)payload);
    y(free);
    setlocale(LC_NUMERIC, "");
}

/*
 * For the binding events, we send the serialized binding struct.
 */
void ipc_send_binding_event(const char *event_type, Binding *bind) {
    DLOG("Issue IPC binding %s event (sym = %s, code = %d)\n", event_type, bind->symbol, bind->keycode);

    setlocale(LC_NUMERIC, "C");

    yajl_gen gen = ygenalloc();

    y(map_open);

    ystr("change");
    ystr(event_type);

    ystr("binding");
    dump_binding(gen, bind);

    y(map_close);

    const unsigned char *payload;
    ylength length;
    y(get_buf, &payload, &length);

    ipc_send_event("binding", I3_IPC_EVENT_BINDING, (const char *)payload);

    y(free);
    setlocale(LC_NUMERIC, "");
}
