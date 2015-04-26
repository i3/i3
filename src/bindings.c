/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * bindings.c: Functions for configuring, finding and, running bindings.
 */
#include "all.h"

#include <xkbcommon/xkbcommon.h>

pid_t command_error_nagbar_pid = -1;

/*
 * The name of the default mode.
 *
 */
const char *DEFAULT_BINDING_MODE = "default";

/*
 * Returns the mode specified by `name` or creates a new mode and adds it to
 * the list of modes.
 *
 */
static struct Mode *mode_from_name(const char *name) {
    struct Mode *mode;

    /* Try to find the mode in the list of modes and return it */
    SLIST_FOREACH(mode, &modes, modes) {
        if (strcmp(mode->name, name) == 0)
            return mode;
    }

    /* If the mode was not found, create a new one */
    mode = scalloc(sizeof(struct Mode));
    mode->name = sstrdup(name);
    mode->bindings = scalloc(sizeof(struct bindings_head));
    TAILQ_INIT(mode->bindings);
    SLIST_INSERT_HEAD(&modes, mode, modes);

    return mode;
}

/*
 * Adds a binding from config parameters given as strings and returns a
 * pointer to the binding structure. Returns NULL if the input code could not
 * be parsed.
 *
 */
Binding *configure_binding(const char *bindtype, const char *modifiers, const char *input_code,
                           const char *release, const char *border, const char *whole_window,
                           const char *command, const char *modename) {
    Binding *new_binding = scalloc(sizeof(Binding));
    DLOG("bindtype %s, modifiers %s, input code %s, release %s\n", bindtype, modifiers, input_code, release);
    new_binding->release = (release != NULL ? B_UPON_KEYRELEASE : B_UPON_KEYPRESS);
    new_binding->border = (border != NULL);
    new_binding->whole_window = (whole_window != NULL);
    if (strcmp(bindtype, "bindsym") == 0) {
        new_binding->input_type = (strncasecmp(input_code, "button", (sizeof("button") - 1)) == 0
                                       ? B_MOUSE
                                       : B_KEYBOARD);

        new_binding->symbol = sstrdup(input_code);
    } else {
        // TODO: strtol with proper error handling
        new_binding->keycode = atoi(input_code);
        new_binding->input_type = B_KEYBOARD;
        if (new_binding->keycode == 0) {
            ELOG("Could not parse \"%s\" as an input code, ignoring this binding.\n", input_code);
            FREE(new_binding);
            return NULL;
        }
    }
    new_binding->mods = modifiers_from_str(modifiers);
    new_binding->command = sstrdup(command);

    struct Mode *mode = mode_from_name(modename);
    TAILQ_INSERT_TAIL(mode->bindings, new_binding, bindings);

    return new_binding;
}

static void grab_keycode_for_binding(xcb_connection_t *conn, Binding *bind, uint32_t keycode) {
    if (bind->input_type != B_KEYBOARD)
        return;

    DLOG("Grabbing %d with modifiers %d (with mod_mask_lock %d)\n", keycode, bind->mods, bind->mods | XCB_MOD_MASK_LOCK);
/* Grab the key in all combinations */
#define GRAB_KEY(modifier)                                                                       \
    do {                                                                                         \
        xcb_grab_key(conn, 0, root, modifier, keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC); \
    } while (0)
    int mods = bind->mods;
    if ((bind->mods & BIND_MODE_SWITCH) != 0) {
        mods &= ~BIND_MODE_SWITCH;
        if (mods == 0)
            mods = XCB_MOD_MASK_ANY;
    }
    GRAB_KEY(mods);
    GRAB_KEY(mods | xcb_numlock_mask);
    GRAB_KEY(mods | XCB_MOD_MASK_LOCK);
    GRAB_KEY(mods | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
}

/*
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn, bool bind_mode_switch) {
    Binding *bind;
    TAILQ_FOREACH(bind, bindings, bindings) {
        if (bind->input_type != B_KEYBOARD ||
            (bind_mode_switch && (bind->mods & BIND_MODE_SWITCH) == 0) ||
            (!bind_mode_switch && (bind->mods & BIND_MODE_SWITCH) != 0))
            continue;

        /* The easy case: the user specified a keycode directly. */
        if (bind->keycode > 0) {
            grab_keycode_for_binding(conn, bind, bind->keycode);
            continue;
        }

        xcb_keycode_t *walk = bind->translated_to;
        for (uint32_t i = 0; i < bind->number_keycodes; i++)
            grab_keycode_for_binding(conn, bind, *walk++);
    }
}

/*
 * Returns a pointer to the Binding with the specified modifiers and
 * keycode or NULL if no such binding exists.
 *
 */
static Binding *get_binding(uint16_t modifiers, bool is_release, uint16_t input_code, input_type_t input_type) {
    Binding *bind;

    if (!is_release) {
        /* On a press event, we first reset all B_UPON_KEYRELEASE_IGNORE_MODS
         * bindings back to B_UPON_KEYRELEASE */
        TAILQ_FOREACH(bind, bindings, bindings) {
            if (bind->input_type != input_type)
                continue;
            if (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS)
                bind->release = B_UPON_KEYRELEASE;
        }
    }

    TAILQ_FOREACH(bind, bindings, bindings) {
        /* First compare the modifiers (unless this is a
         * B_UPON_KEYRELEASE_IGNORE_MODS binding and this is a KeyRelease
         * event) */
        if (bind->input_type != input_type)
            continue;
        if (bind->mods != modifiers &&
            (bind->release != B_UPON_KEYRELEASE_IGNORE_MODS ||
             !is_release))
            continue;

        /* For keyboard bindings where a symbol was specified by the user, we
         * need to look in the array of translated keycodes for the event’s
         * keycode */
        if (input_type == B_KEYBOARD && bind->symbol != NULL) {
            xcb_keycode_t input_keycode = (xcb_keycode_t)input_code;
            if (memmem(bind->translated_to,
                       bind->number_keycodes * sizeof(xcb_keycode_t),
                       &input_keycode, sizeof(xcb_keycode_t)) == NULL)
                continue;
        } else {
            /* This case is easier: The user specified a keycode */
            if (bind->keycode != input_code)
                continue;
        }

        /* If this binding is a release binding, it matches the key which the
         * user pressed. We therefore mark it as B_UPON_KEYRELEASE_IGNORE_MODS
         * for later, so that the user can release the modifiers before the
         * actual key or button and the release event will still be matched. */
        if (bind->release == B_UPON_KEYRELEASE && !is_release)
            bind->release = B_UPON_KEYRELEASE_IGNORE_MODS;

        /* Check if the binding is for a press or a release event */
        if ((bind->release == B_UPON_KEYPRESS && is_release) ||
            (bind->release >= B_UPON_KEYRELEASE && !is_release))
            continue;

        break;
    }

    return (bind == TAILQ_END(bindings) ? NULL : bind);
}

/*
 * Returns a pointer to the Binding that matches the given xcb button or key
 * event or NULL if no such binding exists.
 *
 */
Binding *get_binding_from_xcb_event(xcb_generic_event_t *event) {
    bool is_release = (event->response_type == XCB_KEY_RELEASE || event->response_type == XCB_BUTTON_RELEASE);

    input_type_t input_type = ((event->response_type == XCB_BUTTON_RELEASE || event->response_type == XCB_BUTTON_PRESS)
                                   ? B_MOUSE
                                   : B_KEYBOARD);

    uint16_t event_state = ((xcb_key_press_event_t *)event)->state;
    uint16_t event_detail = ((xcb_key_press_event_t *)event)->detail;

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    uint16_t state_filtered = event_state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    DLOG("(removed numlock, state = %d)\n", state_filtered);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     * button masks are filtered out */
    state_filtered &= 0xFF;
    DLOG("(removed upper 8 bits, state = %d)\n", state_filtered);

    if (xkb_current_group == XCB_XKB_GROUP_2)
        state_filtered |= BIND_MODE_SWITCH;

    DLOG("(checked mode_switch, state %d)\n", state_filtered);

    /* Find the binding */
    Binding *bind = get_binding(state_filtered, is_release, event_detail, input_type);

    /* No match? Then the user has Mode_switch enabled but does not have a
     * specific keybinding. Fall back to the default keybindings (without
     * Mode_switch). Makes it much more convenient for users of a hybrid
     * layout (like ru). */
    if (bind == NULL) {
        state_filtered &= ~(BIND_MODE_SWITCH);
        DLOG("no match, new state_filtered = %d\n", state_filtered);
        if ((bind = get_binding(state_filtered, is_release, event_detail, input_type)) == NULL) {
            /* This is not a real error since we can have release and
             * non-release bindings. On a press event for which there is only a
             * !release-binding, but no release-binding, the corresponding
             * release event will trigger this. No problem, though. */
            DLOG("Could not lookup key binding (modifiers %d, keycode %d)\n",
                 state_filtered, event_detail);
        }
    }

    return bind;
}

/*
 * Translates keysymbols to keycodes for all bindings which use keysyms.
 *
 */
void translate_keysyms(void) {
    Binding *bind;
    xcb_keysym_t keysym;
    int col;
    xcb_keycode_t i, min_keycode, max_keycode;

    min_keycode = xcb_get_setup(conn)->min_keycode;
    max_keycode = xcb_get_setup(conn)->max_keycode;

    TAILQ_FOREACH(bind, bindings, bindings) {
        if (bind->input_type == B_MOUSE) {
            int button = atoi(bind->symbol + (sizeof("button") - 1));
            bind->keycode = button;

            if (button < 1)
                ELOG("Could not translate string to button: \"%s\"\n", bind->symbol);

            continue;
        }

        if (bind->keycode > 0)
            continue;

        /* We need to translate the symbol to a keycode */
        keysym = xkb_keysym_from_name(bind->symbol, XKB_KEYSYM_NO_FLAGS);
        if (keysym == XKB_KEY_NoSymbol) {
            ELOG("Could not translate string to key symbol: \"%s\"\n",
                 bind->symbol);
            continue;
        }

        /* Base column we use for looking up key symbols. We always consider
         * the base column and the corresponding shift column, so without
         * mode_switch, we look in 0 and 1, with mode_switch we look in 2 and
         * 3. */
        col = (bind->mods & BIND_MODE_SWITCH ? 2 : 0);

        FREE(bind->translated_to);
        bind->number_keycodes = 0;

        for (i = min_keycode; i && i <= max_keycode; i++) {
            if ((xcb_key_symbols_get_keysym(keysyms, i, col) != keysym) &&
                (xcb_key_symbols_get_keysym(keysyms, i, col + 1) != keysym))
                continue;
            bind->number_keycodes++;
            bind->translated_to = srealloc(bind->translated_to,
                                           (sizeof(xcb_keycode_t) *
                                            bind->number_keycodes));
            bind->translated_to[bind->number_keycodes - 1] = i;
        }

        DLOG("Translated symbol \"%s\" to %d keycode (mods %d)\n", bind->symbol,
             bind->number_keycodes, bind->mods);
    }
}

/*
 * Switches the key bindings to the given mode, if the mode exists
 *
 */
void switch_mode(const char *new_mode) {
    struct Mode *mode;

    DLOG("Switching to mode %s\n", new_mode);

    SLIST_FOREACH(mode, &modes, modes) {
        if (strcasecmp(mode->name, new_mode) != 0)
            continue;

        ungrab_all_keys(conn);
        bindings = mode->bindings;
        translate_keysyms();
        grab_all_keys(conn, false);

        char *event_msg;
        sasprintf(&event_msg, "{\"change\":\"%s\"}", mode->name);

        ipc_send_event("mode", I3_IPC_EVENT_MODE, event_msg);
        FREE(event_msg);

        return;
    }

    ELOG("ERROR: Mode not found\n");
}

/*
 * Checks for duplicate key bindings (the same keycode or keysym is configured
 * more than once). If a duplicate binding is found, a message is printed to
 * stderr and the has_errors variable is set to true, which will start
 * i3-nagbar.
 *
 */
void check_for_duplicate_bindings(struct context *context) {
    Binding *bind, *current;
    TAILQ_FOREACH(current, bindings, bindings) {
        TAILQ_FOREACH(bind, bindings, bindings) {
            /* Abort when we reach the current keybinding, only check the
             * bindings before */
            if (bind == current)
                break;

            /* Check if the input types are different */
            if (bind->input_type != current->input_type)
                continue;

            /* Check if one is using keysym while the other is using bindsym.
             * If so, skip. */
            /* XXX: It should be checked at a later place (when translating the
             * keysym to keycodes) if there are any duplicates */
            if ((bind->symbol == NULL && current->symbol != NULL) ||
                (bind->symbol != NULL && current->symbol == NULL))
                continue;

            /* If bind is NULL, current has to be NULL, too (see above).
             * If the keycodes differ, it can't be a duplicate. */
            if (bind->symbol != NULL &&
                strcasecmp(bind->symbol, current->symbol) != 0)
                continue;

            /* Check if the keycodes or modifiers are different. If so, they
             * can't be duplicate */
            if (bind->keycode != current->keycode ||
                bind->mods != current->mods ||
                bind->release != current->release)
                continue;

            context->has_errors = true;
            if (current->keycode != 0) {
                ELOG("Duplicate keybinding in config file:\n  modmask %d with keycode %d, command \"%s\"\n",
                     current->mods, current->keycode, current->command);
            } else {
                ELOG("Duplicate keybinding in config file:\n  modmask %d with keysym %s, command \"%s\"\n",
                     current->mods, current->symbol, current->command);
            }
        }
    }
}

/*
 * Creates a dynamically allocated copy of bind.
 */
static Binding *binding_copy(Binding *bind) {
    Binding *ret = smalloc(sizeof(Binding));
    *ret = *bind;
    if (bind->symbol != NULL)
        ret->symbol = strdup(bind->symbol);
    if (bind->command != NULL)
        ret->command = strdup(bind->command);
    if (bind->translated_to != NULL) {
        ret->translated_to = smalloc(sizeof(xcb_keycode_t) * bind->number_keycodes);
        memcpy(ret->translated_to, bind->translated_to, sizeof(xcb_keycode_t) * bind->number_keycodes);
    }
    return ret;
}

/*
 * Frees the binding. If bind is null, it simply returns.
 */
void binding_free(Binding *bind) {
    if (bind == NULL) {
        return;
    }

    FREE(bind->symbol);
    FREE(bind->translated_to);
    FREE(bind->command);
    FREE(bind);
}

/*
 * Runs the given binding and handles parse errors. If con is passed, it will
 * execute the command binding with that container selected by criteria.
 * Returns a CommandResult for running the binding's command. Caller should
 * render tree if needs_tree_render is true. Free with command_result_free().
 *
 */
CommandResult *run_binding(Binding *bind, Con *con) {
    char *command;

    /* We need to copy the binding and command since “reload” may be part of
     * the command, and then the memory that bind points to may not contain the
     * same data anymore. */
    if (con == NULL)
        command = sstrdup(bind->command);
    else
        sasprintf(&command, "[con_id=\"%p\"] %s", con, bind->command);

    Binding *bind_cp = binding_copy(bind);
    CommandResult *result = parse_command(command, NULL);
    free(command);

    if (result->needs_tree_render)
        tree_render();

    if (result->parse_error) {
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
            NULL};
        start_nagbar(&command_error_nagbar_pid, argv);
        free(pageraction);
    }

    ipc_send_binding_event("run", bind_cp);
    binding_free(bind_cp);

    return result;
}
