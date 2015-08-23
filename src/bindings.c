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
#include <xkbcommon/xkbcommon-x11.h>

static struct xkb_context *xkb_context;
static struct xkb_keymap *xkb_keymap;

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
    mode = scalloc(1, sizeof(struct Mode));
    mode->name = sstrdup(name);
    mode->bindings = scalloc(1, sizeof(struct bindings_head));
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
    Binding *new_binding = scalloc(1, sizeof(Binding));
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
        char *endptr;
        long keycode = strtol(input_code, &endptr, 10);
        new_binding->keycode = keycode;
        new_binding->input_type = B_KEYBOARD;
        if (keycode == LONG_MAX || keycode == LONG_MIN || keycode < 0 || *endptr != '\0' || endptr == input_code) {
            ELOG("Could not parse \"%s\" as an input code, ignoring this binding.\n", input_code);
            FREE(new_binding);
            return NULL;
        }
    }
    new_binding->command = sstrdup(command);
    new_binding->event_state_mask = event_state_from_str(modifiers);
    int group_bits_set = 0;
    if (new_binding->event_state_mask & I3_XKB_GROUP_MASK_1)
        group_bits_set++;
    if (new_binding->event_state_mask & I3_XKB_GROUP_MASK_2)
        group_bits_set++;
    if (new_binding->event_state_mask & I3_XKB_GROUP_MASK_3)
        group_bits_set++;
    if (new_binding->event_state_mask & I3_XKB_GROUP_MASK_4)
        group_bits_set++;
    if (group_bits_set > 1)
        ELOG("Keybinding has more than one Group specified, but your X server is always in precisely one group. The keybinding can never trigger.\n");

    struct Mode *mode = mode_from_name(modename);
    TAILQ_INSERT_TAIL(mode->bindings, new_binding, bindings);

    return new_binding;
}

static void grab_keycode_for_binding(xcb_connection_t *conn, Binding *bind, uint32_t keycode) {
    if (bind->input_type != B_KEYBOARD)
        return;

/* Grab the key in all combinations */
#define GRAB_KEY(modifier)                                                                       \
    do {                                                                                         \
        xcb_grab_key(conn, 0, root, modifier, keycode, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC); \
    } while (0)
    int mods = bind->event_state_mask;
    if (((mods >> 16) & I3_XKB_GROUP_MASK_1) && xkb_current_group != XCB_XKB_GROUP_1)
        return;
    if (((mods >> 16) & I3_XKB_GROUP_MASK_2) && xkb_current_group != XCB_XKB_GROUP_2)
        return;
    if (((mods >> 16) & I3_XKB_GROUP_MASK_3) && xkb_current_group != XCB_XKB_GROUP_3)
        return;
    if (((mods >> 16) & I3_XKB_GROUP_MASK_4) && xkb_current_group != XCB_XKB_GROUP_4)
        return;
    mods &= 0xFFFF;
    DLOG("Grabbing keycode %d with event state mask 0x%x (mods 0x%x)\n",
         keycode, bind->event_state_mask, mods);
    GRAB_KEY(mods);
    GRAB_KEY(mods | xcb_numlock_mask);
    GRAB_KEY(mods | XCB_MOD_MASK_LOCK);
    GRAB_KEY(mods | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
}

/*
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn) {
    Binding *bind;
    TAILQ_FOREACH(bind, bindings, bindings) {
        if (bind->input_type != B_KEYBOARD)
            continue;

        /* The easy case: the user specified a keycode directly. */
        if (bind->keycode > 0) {
            grab_keycode_for_binding(conn, bind, bind->keycode);
            continue;
        }

        for (uint32_t i = 0; i < bind->number_keycodes; i++)
            grab_keycode_for_binding(conn, bind, bind->translated_to[i]);
    }
}

/*
 * Returns a pointer to the Binding with the specified modifiers and
 * keycode or NULL if no such binding exists.
 *
 */
static Binding *get_binding(i3_event_state_mask_t state_filtered, bool is_release, uint16_t input_code, input_type_t input_type) {
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
        DLOG("binding with event_state_mask 0x%x, state_filtered 0x%x, match: %s\n",
             bind->event_state_mask, state_filtered,
             ((state_filtered & bind->event_state_mask) == bind->event_state_mask) ? "yes" : "no");
        /* First compare the state_filtered (unless this is a
         * B_UPON_KEYRELEASE_IGNORE_MODS binding and this is a KeyRelease
         * event) */
        if (bind->input_type != input_type)
            continue;
        if ((state_filtered & bind->event_state_mask) != bind->event_state_mask &&
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
    const bool is_release = (event->response_type == XCB_KEY_RELEASE ||
                             event->response_type == XCB_BUTTON_RELEASE);

    const input_type_t input_type = ((event->response_type == XCB_BUTTON_RELEASE ||
                                      event->response_type == XCB_BUTTON_PRESS)
                                         ? B_MOUSE
                                         : B_KEYBOARD);

    const uint16_t event_state = ((xcb_key_press_event_t *)event)->state;
    const uint16_t event_detail = ((xcb_key_press_event_t *)event)->detail;

    /* Remove the numlock bit */
    i3_event_state_mask_t state_filtered = event_state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    DLOG("(removed numlock, state = 0x%x)\n", state_filtered);
    /* Transform the keyboard_group from bit 13 and bit 14 into an
     * i3_xkb_group_mask_t, so that get_binding() can just bitwise AND the
     * configured bindings against |state_filtered|.
     *
     * These bits are only set because we set the XKB client flags
     * XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE and
     * XCB_XKB_PER_CLIENT_FLAG_LOOKUP_STATE_WHEN_GRABBED. See also doc/kbproto
     * section 2.2.2:
     * http://www.x.org/releases/X11R7.7/doc/kbproto/xkbproto.html#Computing_A_State_Field_from_an_XKB_State */
    switch ((event_state & 0x6000) >> 13) {
        case XCB_XKB_GROUP_1:
            state_filtered |= (I3_XKB_GROUP_MASK_1 << 16);
            break;
        case XCB_XKB_GROUP_2:
            state_filtered |= (I3_XKB_GROUP_MASK_2 << 16);
            break;
        case XCB_XKB_GROUP_3:
            state_filtered |= (I3_XKB_GROUP_MASK_3 << 16);
            break;
        case XCB_XKB_GROUP_4:
            state_filtered |= (I3_XKB_GROUP_MASK_4 << 16);
            break;
    }
    state_filtered &= ~0x6000;
    DLOG("(transformed keyboard group, state = 0x%x)\n", state_filtered);
    return get_binding(state_filtered, is_release, event_detail, input_type);
}

struct resolve {
    /* The binding which we are resolving. */
    Binding *bind;

    /* |bind|’s keysym (translated to xkb_keysym_t), e.g. XKB_KEY_R. */
    xkb_keysym_t keysym;

    /* The xkb state built from the user-provided modifiers and group. */
    struct xkb_state *xkb_state;
};

/*
 * add_keycode_if_matches is called for each keycode in the keymap and will add
 * the keycode to |data->bind| if the keycode can result in the keysym
 * |data->resolving|.
 *
 */
static void add_keycode_if_matches(struct xkb_keymap *keymap, xkb_keycode_t key, void *data) {
    const struct resolve *resolving = data;
    const xkb_keysym_t *syms;
    const int num_keysyms = xkb_state_key_get_syms(resolving->xkb_state, key, &syms);
    for (int n = 0; n < num_keysyms; n++) {
        if (syms[n] != resolving->keysym)
            continue;
        Binding *bind = resolving->bind;
        bind->number_keycodes++;
        bind->translated_to = srealloc(bind->translated_to,
                                       (sizeof(xcb_keycode_t) *
                                        bind->number_keycodes));
        bind->translated_to[bind->number_keycodes - 1] = key;
        break;
    }
}

/*
 * Translates keysymbols to keycodes for all bindings which use keysyms.
 *
 */
void translate_keysyms(void) {
    struct xkb_state *dummy_state = xkb_state_new(xkb_keymap);
    if (dummy_state == NULL) {
        ELOG("Could not create XKB state, cannot translate keysyms.\n");
        return;
    }

    Binding *bind;
    TAILQ_FOREACH(bind, bindings, bindings) {
        if (bind->input_type == B_MOUSE) {
            char *endptr;
            long button = strtol(bind->symbol + (sizeof("button") - 1), &endptr, 10);
            bind->keycode = button;

            if (button == LONG_MAX || button == LONG_MIN || button < 0 || *endptr != '\0' || endptr == bind->symbol)
                ELOG("Could not translate string to button: \"%s\"\n", bind->symbol);

            continue;
        }

        if (bind->keycode > 0)
            continue;

        /* We need to translate the symbol to a keycode */
        const xkb_keysym_t keysym = xkb_keysym_from_name(bind->symbol, XKB_KEYSYM_NO_FLAGS);
        if (keysym == XKB_KEY_NoSymbol) {
            ELOG("Could not translate string to key symbol: \"%s\"\n",
                 bind->symbol);
            continue;
        }

        xkb_layout_index_t group = XCB_XKB_GROUP_1;
        if ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_2)
            group = XCB_XKB_GROUP_2;
        else if ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_3)
            group = XCB_XKB_GROUP_3;
        else if ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_4)
            group = XCB_XKB_GROUP_4;

        DLOG("group = %d, event_state_mask = %d, &2 = %s, &3 = %s, &4 = %s\n", group,
             bind->event_state_mask,
             (bind->event_state_mask & I3_XKB_GROUP_MASK_2) ? "yes" : "no",
             (bind->event_state_mask & I3_XKB_GROUP_MASK_3) ? "yes" : "no",
             (bind->event_state_mask & I3_XKB_GROUP_MASK_4) ? "yes" : "no");
        (void)xkb_state_update_mask(
            dummy_state,
            (bind->event_state_mask & 0x1FFF) /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        struct resolve resolving = {
            .bind = bind,
            .keysym = keysym,
            .xkb_state = dummy_state,
        };
        FREE(bind->translated_to);
        bind->number_keycodes = 0;
        xkb_keymap_key_for_each(xkb_keymap, add_keycode_if_matches, &resolving);
        char *keycodes = sstrdup("");
        for (uint32_t n = 0; n < bind->number_keycodes; n++) {
            char *tmp;
            sasprintf(&tmp, "%s %d", keycodes, bind->translated_to[n]);
            free(keycodes);
            keycodes = tmp;
        }
        DLOG("state=0x%x, cfg=\"%s\", sym=0x%x → keycodes%s (%d)\n",
             bind->event_state_mask, bind->symbol, keysym, keycodes, bind->number_keycodes);
        free(keycodes);
    }

    xkb_state_unref(dummy_state);
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
        grab_all_keys(conn);

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
                bind->event_state_mask != current->event_state_mask ||
                bind->release != current->release)
                continue;

            context->has_errors = true;
            if (current->keycode != 0) {
                ELOG("Duplicate keybinding in config file:\n  state mask 0x%x with keycode %d, command \"%s\"\n",
                     current->event_state_mask, current->keycode, current->command);
            } else {
                ELOG("Duplicate keybinding in config file:\n  state mask 0x%x with keysym %s, command \"%s\"\n",
                     current->event_state_mask, current->symbol, current->command);
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
        ret->symbol = sstrdup(bind->symbol);
    if (bind->command != NULL)
        ret->command = sstrdup(bind->command);
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

/*
 * Loads the XKB keymap from the X11 server and feeds it to xkbcommon.
 *
 */
bool load_keymap(void) {
    if (xkb_context == NULL) {
        if ((xkb_context = xkb_context_new(0)) == NULL) {
            ELOG("Could not create xkbcommon context\n");
            return false;
        }
    }

    struct xkb_keymap *new_keymap;
    const int32_t device_id = xkb_x11_get_core_keyboard_device_id(conn);
    DLOG("device_id = %d\n", device_id);
    if ((new_keymap = xkb_x11_keymap_new_from_device(xkb_context, conn, device_id, 0)) == NULL) {
        ELOG("xkb_x11_keymap_new_from_device failed\n");
        return false;
    }
    xkb_keymap_unref(xkb_keymap);
    xkb_keymap = new_keymap;

    return true;
}
