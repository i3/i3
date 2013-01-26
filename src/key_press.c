#undef I3__FILE__
#define I3__FILE__ "key_press.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2013 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * key_press.c: key press handler
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "all.h"

static int current_nesting_level;
static bool parse_error_key;
static bool command_failed;

pid_t command_error_nagbar_pid = -1;

static int json_boolean(void *ctx, int boolval) {
    DLOG("Got bool: %d, parse_error_key %d, nesting_level %d\n", boolval, parse_error_key, current_nesting_level);

    if (parse_error_key && current_nesting_level == 1 && boolval)
        command_failed = true;

    return 1;
}

#if YAJL_MAJOR >= 2
static int json_map_key(void *ctx, const unsigned char *stringval, size_t stringlen) {
#else
static int json_map_key(void *ctx, const unsigned char *stringval, unsigned int stringlen) {
#endif
    parse_error_key = (stringlen >= strlen("parse_error") &&
                       strncmp((const char*)stringval, "parse_error", strlen("parse_error")) == 0);
    return 1;
}

static int json_start_map(void *ctx) {
    current_nesting_level++;
    return 1;
}

static int json_end_map(void *ctx) {
    current_nesting_level--;
    return 1;
}

static yajl_callbacks command_error_callbacks = {
    NULL,
    &json_boolean,
    NULL,
    NULL,
    NULL,
    NULL,
    &json_start_map,
    &json_map_key,
    &json_end_map,
    NULL,
    NULL
};

/*
 * There was a KeyPress or KeyRelease (both events have the same fields). We
 * compare this key code with our bindings table and pass the bound action to
 * parse_command().
 *
 */
void handle_key_press(xcb_key_press_event_t *event) {
    bool key_release = (event->response_type == XCB_KEY_RELEASE);

    last_timestamp = event->time;

    DLOG("%s %d, state raw = %d\n", (key_release ? "KeyRelease" : "KeyPress"), event->detail, event->state);

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    DLOG("(removed numlock, state = %d)\n", state_filtered);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     * button masks are filtered out */
    state_filtered &= 0xFF;
    DLOG("(removed upper 8 bits, state = %d)\n", state_filtered);

    if (xkb_current_group == XkbGroup2Index)
        state_filtered |= BIND_MODE_SWITCH;

    DLOG("(checked mode_switch, state %d)\n", state_filtered);

    /* Find the binding */
    Binding *bind = get_binding(state_filtered, key_release, event->detail);

    /* No match? Then the user has Mode_switch enabled but does not have a
     * specific keybinding. Fall back to the default keybindings (without
     * Mode_switch). Makes it much more convenient for users of a hybrid
     * layout (like us, ru). */
    if (bind == NULL) {
        state_filtered &= ~(BIND_MODE_SWITCH);
        DLOG("no match, new state_filtered = %d\n", state_filtered);
        if ((bind = get_binding(state_filtered, key_release, event->detail)) == NULL) {
            /* This is not a real error since we can have release and
             * non-release keybindings. On a KeyPress event for which there is
             * only a !release-binding, but no release-binding, the
             * corresponding KeyRelease event will trigger this. No problem,
             * though. */
            DLOG("Could not lookup key binding (modifiers %d, keycode %d)\n",
                 state_filtered, event->detail);
            return;
        }
    }

    char *command_copy = sstrdup(bind->command);
    struct CommandResult *command_output = parse_command(command_copy);
    free(command_copy);

    if (command_output->needs_tree_render)
        tree_render();

    /* We parse the JSON reply to figure out whether there was an error
     * ("success" being false in on of the returned dictionaries). */
    const unsigned char *reply;
#if YAJL_MAJOR >= 2
    size_t length;
    yajl_handle handle = yajl_alloc(&command_error_callbacks, NULL, NULL);
#else
    unsigned int length;
    yajl_parser_config parse_conf = { 0, 0 };

    yajl_handle handle = yajl_alloc(&command_error_callbacks, &parse_conf, NULL, NULL);
#endif
    yajl_gen_get_buf(command_output->json_gen, &reply, &length);

    current_nesting_level = 0;
    parse_error_key = false;
    command_failed = false;
    yajl_status state = yajl_parse(handle, reply, length);
    if (state != yajl_status_ok) {
        ELOG("Could not parse my own reply. That's weird. reply is %.*s\n", (int)length, reply);
    } else {
        if (command_failed) {
            char *pageraction;
            sasprintf(&pageraction, "i3-sensible-pager \"%s\"\n", errorfilename);
            char *argv[] = {
                NULL, /* will be replaced by the executable path */
                "-f",
                config.font.pattern,
                "-t",
                "error",
                "-m",
                "The configured command for this shortcut could not be run successfully.",
                "-b",
                "show errors",
                pageraction,
                NULL
            };
            start_nagbar(&command_error_nagbar_pid, argv);
            free(pageraction);
        }
    }

    yajl_free(handle);

    yajl_gen_free(command_output->json_gen);
}
