/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * bindings.c: Functions for configuring, finding and, running bindings.
 */
#include "all.h"

#include <math.h>

#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

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
static struct Mode *mode_from_name(const char *name, bool pango_markup) {
    struct Mode *mode;

    /* Try to find the mode in the list of modes and return it */
    SLIST_FOREACH (mode, &modes, modes) {
        if (strcmp(mode->name, name) == 0) {
            return mode;
        }
    }

    /* If the mode was not found, create a new one */
    mode = scalloc(1, sizeof(struct Mode));
    mode->name = sstrdup(name);
    mode->pango_markup = pango_markup;
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
                           const char *exclude_titlebar, const char *command, const char *modename,
                           bool pango_markup) {
    Binding *new_binding = scalloc(1, sizeof(Binding));
    DLOG("Binding %p bindtype %s, modifiers %s, input code %s, release %s\n", new_binding, bindtype, modifiers, input_code, release);
    new_binding->release = (release != NULL ? B_UPON_KEYRELEASE : B_UPON_KEYPRESS);
    new_binding->border = (border != NULL);
    new_binding->whole_window = (whole_window != NULL);
    new_binding->exclude_titlebar = (exclude_titlebar != NULL);
    if (strcmp(bindtype, "bindsym") == 0) {
        new_binding->input_type = (strncasecmp(input_code, "button", (sizeof("button") - 1)) == 0
                                       ? B_MOUSE
                                       : B_KEYBOARD);

        new_binding->symbol = sstrdup(input_code);
    } else {
        long keycode;
        if (!parse_long(input_code, &keycode, 10)) {
            ELOG("Could not parse \"%s\" as an input code, ignoring this binding.\n", input_code);
            FREE(new_binding);
            return NULL;
        }

        new_binding->keycode = keycode;
        new_binding->input_type = B_KEYBOARD;
    }
    new_binding->command = sstrdup(command);
    new_binding->event_state_mask = event_state_from_str(modifiers);
    int group_bits_set = 0;
    if ((new_binding->event_state_mask >> 16) & I3_XKB_GROUP_MASK_1) {
        group_bits_set++;
    }
    if ((new_binding->event_state_mask >> 16) & I3_XKB_GROUP_MASK_2) {
        group_bits_set++;
    }
    if ((new_binding->event_state_mask >> 16) & I3_XKB_GROUP_MASK_3) {
        group_bits_set++;
    }
    if ((new_binding->event_state_mask >> 16) & I3_XKB_GROUP_MASK_4) {
        group_bits_set++;
    }
    if (group_bits_set > 1) {
        ELOG("Keybinding has more than one Group specified, but your X server is always in precisely one group. The keybinding can never trigger.\n");
    }

    struct Mode *mode = mode_from_name(modename, pango_markup);
    TAILQ_INSERT_TAIL(mode->bindings, new_binding, bindings);

    TAILQ_INIT(&(new_binding->keycodes_head));

    return new_binding;
}

static bool binding_in_current_group(const Binding *bind) {
    /* If no bits are set, the binding should be installed in every group. */
    if ((bind->event_state_mask >> 16) == I3_XKB_GROUP_MASK_ANY) {
        return true;
    }
    switch (xkb_current_group) {
        case XCB_XKB_GROUP_1:
            return ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_1);
        case XCB_XKB_GROUP_2:
            return ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_2);
        case XCB_XKB_GROUP_3:
            return ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_3);
        case XCB_XKB_GROUP_4:
            return ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_4);
        default:
            ELOG("BUG: xkb_current_group (= %d) outside of [XCB_XKB_GROUP_1..XCB_XKB_GROUP_4]\n", xkb_current_group);
            return false;
    }
}

static void grab_keycode_for_binding(xcb_connection_t *conn, Binding *bind, uint32_t keycode) {
    /* Grab the key in all combinations */
#define GRAB_KEY(modifier)                                                                        \
    do {                                                                                          \
        xcb_grab_key(conn, 0, root, modifier, keycode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC); \
    } while (0)
    const int mods = (bind->event_state_mask & 0xFFFF);
    DLOG("Binding %p Grabbing keycode %d with event state mask 0x%x (mods 0x%x)\n",
         bind, keycode, bind->event_state_mask, mods);
    GRAB_KEY(mods);
    /* Also bind the key with active NumLock */
    GRAB_KEY(mods | xcb_numlock_mask);
    /* Also bind the key with active CapsLock */
    GRAB_KEY(mods | XCB_MOD_MASK_LOCK);
    /* Also bind the key with active NumLock+CapsLock */
    GRAB_KEY(mods | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
}

/*
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn) {
    Binding *bind;
    TAILQ_FOREACH (bind, bindings, bindings) {
        if (bind->input_type != B_KEYBOARD) {
            continue;
        }

        if (!binding_in_current_group(bind)) {
            continue;
        }

        /* The easy case: the user specified a keycode directly. */
        if (bind->keycode > 0) {
            grab_keycode_for_binding(conn, bind, bind->keycode);
            continue;
        }

        struct Binding_Keycode *binding_keycode;
        TAILQ_FOREACH (binding_keycode, &(bind->keycodes_head), keycodes) {
            const int keycode = binding_keycode->keycode;
            const int mods = (binding_keycode->modifiers & 0xFFFF);
            DLOG("Binding %p Grabbing keycode %d with mods %d\n", bind, keycode, mods);
            xcb_grab_key(conn, 0, root, mods, keycode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }
    }
}

/*
 * Release the button grabs on all managed windows and regrab them,
 * reevaluating which buttons need to be grabbed.
 *
 */
void regrab_all_buttons(xcb_connection_t *conn) {
    int *buttons = bindings_get_buttons_to_grab();
    xcb_grab_server(conn);

    Con *con;
    TAILQ_FOREACH (con, &all_cons, all_cons) {
        if (con->window == NULL) {
            continue;
        }

        xcb_ungrab_button(conn, XCB_BUTTON_INDEX_ANY, con->window->id, XCB_BUTTON_MASK_ANY);
        xcb_grab_buttons(conn, con->window->id, buttons);
    }

    FREE(buttons);
    xcb_ungrab_server(conn);
}

/*
 * Returns a pointer to the Binding with the specified modifiers and
 * keycode or NULL if no such binding exists.
 *
 */
static Binding *get_binding(i3_event_state_mask_t state_filtered, bool is_release, uint16_t input_code, input_type_t input_type) {
    Binding *bind;
    Binding *result = NULL;

    if (!is_release) {
        /* On a press event, we first reset all B_UPON_KEYRELEASE_IGNORE_MODS
         * bindings back to B_UPON_KEYRELEASE */
        TAILQ_FOREACH (bind, bindings, bindings) {
            if (bind->input_type != input_type) {
                continue;
            }
            if (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS) {
                bind->release = B_UPON_KEYRELEASE;
            }
        }
    }

    const uint32_t xkb_group_state = (state_filtered & 0xFFFF0000);
    const uint32_t modifiers_state = (state_filtered & 0x0000FFFF);
    TAILQ_FOREACH (bind, bindings, bindings) {
        if (bind->input_type != input_type) {
            continue;
        }

        const uint32_t xkb_group_mask = (bind->event_state_mask & 0xFFFF0000);
        const bool groups_match = ((xkb_group_state & xkb_group_mask) == xkb_group_mask);
        if (!groups_match) {
            DLOG("skipping binding %p because XKB groups do not match\n", bind);
            continue;
        }

        /* For keyboard bindings where a symbol was specified by the user, we
         * need to look in the array of translated keycodes for the event’s
         * keycode */
        bool found_keycode = false;
        if (input_type == B_KEYBOARD && bind->symbol != NULL) {
            xcb_keycode_t input_keycode = (xcb_keycode_t)input_code;
            struct Binding_Keycode *binding_keycode;
            TAILQ_FOREACH (binding_keycode, &(bind->keycodes_head), keycodes) {
                const uint32_t modifiers_mask = (binding_keycode->modifiers & 0x0000FFFF);
                const bool mods_match = (modifiers_mask == modifiers_state);
                DLOG("binding_keycode->modifiers = %d, modifiers_mask = %d, modifiers_state = %d, mods_match = %s\n",
                     binding_keycode->modifiers, modifiers_mask, modifiers_state, (mods_match ? "yes" : "no"));
                if (binding_keycode->keycode == input_keycode &&
                    (mods_match || (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS && is_release))) {
                    found_keycode = true;
                    break;
                }
            }
        } else {
            /* This case is easier: The user specified a keycode */
            if (bind->keycode != input_code) {
                continue;
            }

            struct Binding_Keycode *binding_keycode;
            TAILQ_FOREACH (binding_keycode, &(bind->keycodes_head), keycodes) {
                const uint32_t modifiers_mask = (binding_keycode->modifiers & 0x0000FFFF);
                const bool mods_match = (modifiers_mask == modifiers_state);
                DLOG("binding_keycode->modifiers = %d, modifiers_mask = %d, modifiers_state = %d, mods_match = %s\n",
                     binding_keycode->modifiers, modifiers_mask, modifiers_state, (mods_match ? "yes" : "no"));
                if (mods_match || (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS && is_release)) {
                    found_keycode = true;
                    break;
                }
            }
        }
        if (!found_keycode) {
            continue;
        }

        /* If this binding is a release binding, it matches the key which the
         * user pressed. We therefore mark it as B_UPON_KEYRELEASE_IGNORE_MODS
         * for later, so that the user can release the modifiers before the
         * actual key or button and the release event will still be matched. */
        if (bind->release == B_UPON_KEYRELEASE && !is_release) {
            bind->release = B_UPON_KEYRELEASE_IGNORE_MODS;
            DLOG("marked bind %p as B_UPON_KEYRELEASE_IGNORE_MODS\n", bind);
            if (result) {
                break;
            }
            continue;
        }

        /* Check if the binding is for a press or a release event */
        if ((bind->release == B_UPON_KEYPRESS && is_release)) {
            continue;
        }

        if (is_release) {
            return bind;
        } else if (!result) {
            /* Continue looping to mark needed B_UPON_KEYRELEASE_IGNORE_MODS. */
            result = bind;
        }
    }

    return result;
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

    /* Remove the CapsLock bit */
    i3_event_state_mask_t state_filtered = event_state & ~XCB_MOD_MASK_LOCK;
    DLOG("(removed capslock, state = 0x%x)\n", state_filtered);
    /* Transform the keyboard_group from bit 13 and bit 14 into an
     * i3_xkb_group_mask_t, so that get_binding() can just bitwise AND the
     * configured bindings against |state_filtered|.
     *
     * These bits are only set because we set the XKB client flags
     * XCB_XKB_PER_CLIENT_FLAG_GRABS_USE_XKB_STATE and
     * XCB_XKB_PER_CLIENT_FLAG_LOOKUP_STATE_WHEN_GRABBED. See also doc/kbproto
     * section 2.2.2:
     * https://www.x.org/releases/X11R7.7/doc/kbproto/xkbproto.html#Computing_A_State_Field_from_an_XKB_State */
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

    /* Like |xkb_state|, just without the shift modifier, if shift was specified. */
    struct xkb_state *xkb_state_no_shift;

    /* Like |xkb_state|, but with NumLock. */
    struct xkb_state *xkb_state_numlock;

    /* Like |xkb_state|, but with NumLock, just without the shift modifier, if shift was specified. */
    struct xkb_state *xkb_state_numlock_no_shift;
};

#define ADD_TRANSLATED_KEY(code, mods)                                                     \
    do {                                                                                   \
        struct Binding_Keycode *binding_keycode = smalloc(sizeof(struct Binding_Keycode)); \
        binding_keycode->modifiers = (mods);                                               \
        binding_keycode->keycode = (code);                                                 \
        TAILQ_INSERT_TAIL(&(bind->keycodes_head), binding_keycode, keycodes);              \
    } while (0)

/*
 * add_keycode_if_matches is called for each keycode in the keymap and will add
 * the keycode to |data->bind| if the keycode can result in the keysym
 * |data->resolving|.
 *
 */
static void add_keycode_if_matches(struct xkb_keymap *keymap, xkb_keycode_t key, void *data) {
    const struct resolve *resolving = data;
    struct xkb_state *numlock_state = resolving->xkb_state_numlock;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(resolving->xkb_state, key);
    if (sym != resolving->keysym) {
        /* Check if Shift was specified, and try resolving the symbol without
         * shift, so that “bindsym $mod+Shift+a nop” actually works. */
        const xkb_layout_index_t layout = xkb_state_key_get_layout(resolving->xkb_state, key);
        if (layout == XKB_LAYOUT_INVALID) {
            return;
        }
        if (xkb_state_key_get_level(resolving->xkb_state, key, layout) > 1) {
            return;
        }
        /* Skip the Shift fallback for keypad keys, otherwise one cannot bind
         * KP_1 independent of KP_End. */
        if (sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal) {
            return;
        }
        numlock_state = resolving->xkb_state_numlock_no_shift;
        sym = xkb_state_key_get_one_sym(resolving->xkb_state_no_shift, key);
        if (sym != resolving->keysym) {
            return;
        }
    }
    Binding *bind = resolving->bind;

    ADD_TRANSLATED_KEY(key, bind->event_state_mask);

    /* Also bind the key with active CapsLock */
    ADD_TRANSLATED_KEY(key, bind->event_state_mask | XCB_MOD_MASK_LOCK);

    /* If this binding is not explicitly for NumLock, check whether we need to
     * add a fallback. */
    if ((bind->event_state_mask & xcb_numlock_mask) != xcb_numlock_mask) {
        /* Check whether the keycode results in the same keysym when NumLock is
         * active. If so, grab the key with NumLock as well, so that users don’t
         * need to duplicate every key binding with an additional Mod2 specified.
         */
        xkb_keysym_t sym_numlock = xkb_state_key_get_one_sym(numlock_state, key);
        if (sym_numlock == resolving->keysym) {
            /* Also bind the key with active NumLock */
            ADD_TRANSLATED_KEY(key, bind->event_state_mask | xcb_numlock_mask);

            /* Also bind the key with active NumLock+CapsLock */
            ADD_TRANSLATED_KEY(key, bind->event_state_mask | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
        } else {
            DLOG("Skipping automatic numlock fallback, key %d resolves to 0x%x with numlock\n",
                 key, sym_numlock);
        }
    }
}

/*
 * Translates keysymbols to keycodes for all bindings which use keysyms.
 *
 */
void translate_keysyms(void) {
    struct xkb_state *dummy_state = NULL;
    struct xkb_state *dummy_state_no_shift = NULL;
    struct xkb_state *dummy_state_numlock = NULL;
    struct xkb_state *dummy_state_numlock_no_shift = NULL;
    bool has_errors = false;

    if ((dummy_state = xkb_state_new(xkb_keymap)) == NULL ||
        (dummy_state_no_shift = xkb_state_new(xkb_keymap)) == NULL ||
        (dummy_state_numlock = xkb_state_new(xkb_keymap)) == NULL ||
        (dummy_state_numlock_no_shift = xkb_state_new(xkb_keymap)) == NULL) {
        ELOG("Could not create XKB state, cannot translate keysyms.\n");
        goto out;
    }

    Binding *bind;
    TAILQ_FOREACH (bind, bindings, bindings) {
        if (bind->input_type == B_MOUSE) {
            long button;
            if (!parse_long(bind->symbol + (sizeof("button") - 1), &button, 10)) {
                ELOG("Could not translate string to button: \"%s\"\n", bind->symbol);
            }

            xcb_keycode_t key = button;
            bind->keycode = key;
            DLOG("Binding Mouse button, Keycode = %d\n", key);
        }

        xkb_layout_index_t group = XCB_XKB_GROUP_1;
        if ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_2) {
            group = XCB_XKB_GROUP_2;
        } else if ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_3) {
            group = XCB_XKB_GROUP_3;
        } else if ((bind->event_state_mask >> 16) & I3_XKB_GROUP_MASK_4) {
            group = XCB_XKB_GROUP_4;
        }

        DLOG("Binding %p group = %d, event_state_mask = %d, &2 = %s, &3 = %s, &4 = %s\n",
             bind,
             group,
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

        (void)xkb_state_update_mask(
            dummy_state_no_shift,
            (bind->event_state_mask & 0x1FFF) ^ XCB_KEY_BUT_MASK_SHIFT /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        (void)xkb_state_update_mask(
            dummy_state_numlock,
            (bind->event_state_mask & 0x1FFF) | xcb_numlock_mask /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        (void)xkb_state_update_mask(
            dummy_state_numlock_no_shift,
            ((bind->event_state_mask & 0x1FFF) | xcb_numlock_mask) ^ XCB_KEY_BUT_MASK_SHIFT /* xkb_mod_mask_t base_mods, */,
            0 /* xkb_mod_mask_t latched_mods, */,
            0 /* xkb_mod_mask_t locked_mods, */,
            0 /* xkb_layout_index_t base_group, */,
            0 /* xkb_layout_index_t latched_group, */,
            group /* xkb_layout_index_t locked_group, */);

        if (bind->keycode > 0) {
            /* We need to specify modifiers for the keycode binding (numlock
             * fallback). */
            while (!TAILQ_EMPTY(&(bind->keycodes_head))) {
                struct Binding_Keycode *first = TAILQ_FIRST(&(bind->keycodes_head));
                TAILQ_REMOVE(&(bind->keycodes_head), first, keycodes);
                FREE(first);
            }

            ADD_TRANSLATED_KEY(bind->keycode, bind->event_state_mask);

            /* Also bind the key with active CapsLock */
            ADD_TRANSLATED_KEY(bind->keycode, bind->event_state_mask | XCB_MOD_MASK_LOCK);

            /* If this binding is not explicitly for NumLock, check whether we need to
             * add a fallback. */
            if ((bind->event_state_mask & xcb_numlock_mask) != xcb_numlock_mask) {
                /* Check whether the keycode results in the same keysym when NumLock is
                 * active. If so, grab the key with NumLock as well, so that users don’t
                 * need to duplicate every key binding with an additional Mod2 specified.
                 */
                xkb_keysym_t sym = xkb_state_key_get_one_sym(dummy_state, bind->keycode);
                xkb_keysym_t sym_numlock = xkb_state_key_get_one_sym(dummy_state_numlock, bind->keycode);
                if (sym == sym_numlock) {
                    /* Also bind the key with active NumLock */
                    ADD_TRANSLATED_KEY(bind->keycode, bind->event_state_mask | xcb_numlock_mask);

                    /* Also bind the key with active NumLock+CapsLock */
                    ADD_TRANSLATED_KEY(bind->keycode, bind->event_state_mask | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
                } else {
                    DLOG("Skipping automatic numlock fallback, key %d resolves to 0x%x with numlock\n",
                         bind->keycode, sym_numlock);
                }
            }

            continue;
        }

        /* We need to translate the symbol to a keycode */
        const xkb_keysym_t keysym = xkb_keysym_from_name(bind->symbol, XKB_KEYSYM_NO_FLAGS);
        if (keysym == XKB_KEY_NoSymbol) {
            ELOG("Could not translate string to key symbol: \"%s\"\n",
                 bind->symbol);
            continue;
        }

        struct resolve resolving = {
            .bind = bind,
            .keysym = keysym,
            .xkb_state = dummy_state,
            .xkb_state_no_shift = dummy_state_no_shift,
            .xkb_state_numlock = dummy_state_numlock,
            .xkb_state_numlock_no_shift = dummy_state_numlock_no_shift,
        };
        while (!TAILQ_EMPTY(&(bind->keycodes_head))) {
            struct Binding_Keycode *first = TAILQ_FIRST(&(bind->keycodes_head));
            TAILQ_REMOVE(&(bind->keycodes_head), first, keycodes);
            FREE(first);
        }
        xkb_keymap_key_for_each(xkb_keymap, add_keycode_if_matches, &resolving);
        char *keycodes = sstrdup("");
        int num_keycodes = 0;
        struct Binding_Keycode *binding_keycode;
        TAILQ_FOREACH (binding_keycode, &(bind->keycodes_head), keycodes) {
            char *tmp;
            sasprintf(&tmp, "%s %d", keycodes, binding_keycode->keycode);
            free(keycodes);
            keycodes = tmp;
            num_keycodes++;

            /* check for duplicate bindings */
            Binding *check;
            TAILQ_FOREACH (check, bindings, bindings) {
                if (check == bind) {
                    continue;
                }
                if (check->symbol != NULL) {
                    continue;
                }
                if (check->keycode != binding_keycode->keycode ||
                    check->event_state_mask != binding_keycode->modifiers ||
                    check->release != bind->release) {
                    continue;
                }
                has_errors = true;
                ELOG("Duplicate keybinding in config file:\n  keysym = %s, keycode = %d, state_mask = 0x%x\n", bind->symbol, check->keycode, bind->event_state_mask);
            }
        }
        DLOG("state=0x%x, cfg=\"%s\", sym=0x%x → keycodes%s (%d)\n",
             bind->event_state_mask, bind->symbol, keysym, keycodes, num_keycodes);
        free(keycodes);
    }

out:
    xkb_state_unref(dummy_state);
    xkb_state_unref(dummy_state_no_shift);
    xkb_state_unref(dummy_state_numlock);
    xkb_state_unref(dummy_state_numlock_no_shift);

    if (has_errors) {
        start_config_error_nagbar(current_configpath, true);
    }
}

#undef ADD_TRANSLATED_KEY

/*
 * Switches the key bindings to the given mode, if the mode exists
 *
 */
void switch_mode(const char *new_mode) {
    struct Mode *mode;

    DLOG("Switching to mode %s\n", new_mode);

    SLIST_FOREACH (mode, &modes, modes) {
        if (strcmp(mode->name, new_mode) != 0) {
            continue;
        }

        ungrab_all_keys(conn);
        bindings = mode->bindings;
        current_binding_mode = mode->name;
        translate_keysyms();
        grab_all_keys(conn);
        regrab_all_buttons(conn);

        /* Reset all B_UPON_KEYRELEASE_IGNORE_MODS bindings to avoid possibly
         * activating one of them. */
        Binding *bind;
        TAILQ_FOREACH (bind, bindings, bindings) {
            if (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS) {
                bind->release = B_UPON_KEYRELEASE;
            }
        }

        char *event_msg;
        sasprintf(&event_msg, "{\"change\":\"%s\", \"pango_markup\":%s}",
                  mode->name, (mode->pango_markup ? "true" : "false"));

        ipc_send_event("mode", I3_IPC_EVENT_MODE, event_msg);
        FREE(event_msg);

        return;
    }

    ELOG("Mode not found\n");
}

static int reorder_binding_cmp(const void *a, const void *b) {
    Binding *first = *((Binding **)a);
    Binding *second = *((Binding **)b);
    if (first->event_state_mask < second->event_state_mask) {
        return 1;
    } else if (first->event_state_mask == second->event_state_mask) {
        return 0;
    } else {
        return -1;
    }
}

static void reorder_bindings_of_mode(struct Mode *mode) {
    /* Copy the bindings into an array, so that we can use qsort(3). */
    int n = 0;
    Binding *current;
    TAILQ_FOREACH (current, mode->bindings, bindings) {
        n++;
    }
    Binding **tmp = scalloc(n, sizeof(Binding *));
    n = 0;
    TAILQ_FOREACH (current, mode->bindings, bindings) {
        tmp[n++] = current;
    }

    qsort(tmp, n, sizeof(Binding *), reorder_binding_cmp);

    struct bindings_head *reordered = scalloc(1, sizeof(struct bindings_head));
    TAILQ_INIT(reordered);
    for (int i = 0; i < n; i++) {
        current = tmp[i];
        TAILQ_REMOVE(mode->bindings, current, bindings);
        TAILQ_INSERT_TAIL(reordered, current, bindings);
    }
    free(tmp);
    assert(TAILQ_EMPTY(mode->bindings));
    /* Free the old bindings_head, which is now empty. */
    free(mode->bindings);
    mode->bindings = reordered;
}

/*
 * Reorders bindings by event_state_mask descendingly so that get_binding()
 * correctly matches more specific bindings before more generic bindings. Take
 * the following binding configuration as an example:
 *
 *   bindsym n nop lower-case n pressed
 *   bindsym Shift+n nop upper-case n pressed
 *
 * Without reordering, the first binding’s event_state_mask of 0x0 would match
 * the actual event_stat_mask of 0x1 and hence trigger instead of the second
 * keybinding.
 *
 */
void reorder_bindings(void) {
    struct Mode *mode;
    SLIST_FOREACH (mode, &modes, modes) {
        const bool current_mode = (mode->bindings == bindings);
        reorder_bindings_of_mode(mode);
        if (current_mode) {
            bindings = mode->bindings;
        }
    }
}

/*
 * Returns true if a is a key binding for the same key as b.
 *
 */
static bool binding_same_key(Binding *a, Binding *b) {
    /* Check if the input types are different */
    if (a->input_type != b->input_type) {
        return false;
    }

    /* Check if one is using keysym while the other is using bindsym. */
    if ((a->symbol == NULL && b->symbol != NULL) ||
        (a->symbol != NULL && b->symbol == NULL)) {
        return false;
    }

    /* If a is NULL, b has to be NULL, too (see previous conditional).
     * If the keycodes differ, it can't be a duplicate. */
    if (a->symbol != NULL &&
        strcasecmp(a->symbol, b->symbol) != 0) {
        return false;
    }

    /* Check if the keycodes or modifiers are different. If so, they
     * can't be duplicate */
    if (a->keycode != b->keycode ||
        a->event_state_mask != b->event_state_mask ||
        a->release != b->release) {
        return false;
    }

    return true;
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
    TAILQ_FOREACH (current, bindings, bindings) {
        TAILQ_FOREACH (bind, bindings, bindings) {
            /* Abort when we reach the current keybinding, only check the
             * bindings before */
            if (bind == current) {
                break;
            }

            if (!binding_same_key(bind, current)) {
                continue;
            }

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
    if (bind->symbol != NULL) {
        ret->symbol = sstrdup(bind->symbol);
    }
    if (bind->command != NULL) {
        ret->command = sstrdup(bind->command);
    }
    TAILQ_INIT(&(ret->keycodes_head));
    struct Binding_Keycode *binding_keycode;
    TAILQ_FOREACH (binding_keycode, &(bind->keycodes_head), keycodes) {
        struct Binding_Keycode *ret_binding_keycode = smalloc(sizeof(struct Binding_Keycode));
        *ret_binding_keycode = *binding_keycode;
        TAILQ_INSERT_TAIL(&(ret->keycodes_head), ret_binding_keycode, keycodes);
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

    while (!TAILQ_EMPTY(&(bind->keycodes_head))) {
        struct Binding_Keycode *first = TAILQ_FIRST(&(bind->keycodes_head));
        TAILQ_REMOVE(&(bind->keycodes_head), first, keycodes);
        FREE(first);
    }

    FREE(bind->symbol);
    FREE(bind->command);
    FREE(bind);
}

/*
 * Runs the given binding and handles parse errors. If con is passed, it will
 * execute the command binding with that container selected by criteria.
 * Returns a CommandResult for running the binding's command. Free with
 * command_result_free().
 *
 */
CommandResult *run_binding(Binding *bind, Con *con) {
    char *command;

    /* We need to copy the binding and command since “reload” may be part of
     * the command, and then the memory that bind points to may not contain the
     * same data anymore. */
    if (con == NULL) {
        command = sstrdup(bind->command);
    } else {
        sasprintf(&command, "[con_id=\"%p\"] %s", con, bind->command);
    }

    Binding *bind_cp = binding_copy(bind);
    /* The "mode" command might change the current mode, so back it up to
     * correctly produce an event later. */
    char *modename = sstrdup(current_binding_mode);

    CommandResult *result = parse_command(command, NULL, NULL);
    free(command);

    if (result->needs_tree_render) {
        tree_render();
    }

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

    ipc_send_binding_event("run", bind_cp, modename);
    FREE(modename);
    binding_free(bind_cp);

    return result;
}

static int fill_rmlvo_from_root(struct xkb_rule_names *xkb_names) {
    xcb_intern_atom_reply_t *atom_reply;
    size_t content_max_words = 256;

    atom_reply = xcb_intern_atom_reply(
        conn, xcb_intern_atom(conn, 0, strlen("_XKB_RULES_NAMES"), "_XKB_RULES_NAMES"), NULL);
    if (atom_reply == NULL) {
        return -1;
    }

    xcb_get_property_cookie_t prop_cookie;
    xcb_get_property_reply_t *prop_reply;
    prop_cookie = xcb_get_property_unchecked(conn, false, root, atom_reply->atom,
                                             XCB_GET_PROPERTY_TYPE_ANY, 0, content_max_words);
    prop_reply = xcb_get_property_reply(conn, prop_cookie, NULL);
    if (prop_reply == NULL) {
        free(atom_reply);
        return -1;
    }
    if (xcb_get_property_value_length(prop_reply) > 0 && prop_reply->bytes_after > 0) {
        /* We received an incomplete value. Ask again but with a properly
         * adjusted size. */
        content_max_words += ceil(prop_reply->bytes_after / 4.0);
        /* Repeat the request, with adjusted size */
        free(prop_reply);
        prop_cookie = xcb_get_property_unchecked(conn, false, root, atom_reply->atom,
                                                 XCB_GET_PROPERTY_TYPE_ANY, 0, content_max_words);
        prop_reply = xcb_get_property_reply(conn, prop_cookie, NULL);
        if (prop_reply == NULL) {
            free(atom_reply);
            return -1;
        }
    }
    if (xcb_get_property_value_length(prop_reply) == 0) {
        free(atom_reply);
        free(prop_reply);
        return -1;
    }

    const char *walk = (const char *)xcb_get_property_value(prop_reply);
    int remaining = xcb_get_property_value_length(prop_reply);
    for (int i = 0; i < 5 && remaining > 0; i++) {
        const int len = strnlen(walk, remaining);
        switch (i) {
            case 0:
                sasprintf((char **)&(xkb_names->rules), "%.*s", len, walk);
                break;
            case 1:
                sasprintf((char **)&(xkb_names->model), "%.*s", len, walk);
                break;
            case 2:
                sasprintf((char **)&(xkb_names->layout), "%.*s", len, walk);
                break;
            case 3:
                sasprintf((char **)&(xkb_names->variant), "%.*s", len, walk);
                break;
            case 4:
                sasprintf((char **)&(xkb_names->options), "%.*s", len, walk);
                break;
        }
        DLOG("component %d of _XKB_RULES_NAMES is \"%.*s\"\n", i, len, walk);
        walk += (len + 1);
        remaining -= (len + 1);
    }

    free(atom_reply);
    free(prop_reply);
    return 0;
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

    struct xkb_keymap *new_keymap = NULL;
    int32_t device_id;
    if (xkb_supported && (device_id = xkb_x11_get_core_keyboard_device_id(conn)) > -1) {
        if ((new_keymap = xkb_x11_keymap_new_from_device(xkb_context, conn, device_id, 0)) == NULL) {
            ELOG("xkb_x11_keymap_new_from_device failed\n");
            return false;
        }
    } else {
        /* Likely there is no XKB support on this server, possibly because it
         * is a VNC server. */
        LOG("No XKB / core keyboard device? Assembling keymap from local RMLVO.\n");
        struct xkb_rule_names names = {
            .rules = NULL,
            .model = NULL,
            .layout = NULL,
            .variant = NULL,
            .options = NULL};
        if (fill_rmlvo_from_root(&names) == -1) {
            ELOG("Could not get _XKB_RULES_NAMES atom from root window, falling back to defaults.\n");
            /* Using NULL for the fields of xkb_rule_names. */
        }
        new_keymap = xkb_keymap_new_from_names(xkb_context, &names, 0);
        free((char *)names.rules);
        free((char *)names.model);
        free((char *)names.layout);
        free((char *)names.variant);
        free((char *)names.options);
        if (new_keymap == NULL) {
            ELOG("xkb_keymap_new_from_names failed\n");
            return false;
        }
    }
    xkb_keymap_unref(xkb_keymap);
    xkb_keymap = new_keymap;

    return true;
}

/*
 * Returns a list of buttons that should be grabbed on a window.
 * This list will always contain 1–3, all higher buttons will only be returned
 * if there is a whole-window binding for it on some window in the current
 * config.
 * The list is terminated by a 0.
 */
int *bindings_get_buttons_to_grab(void) {
    /* Let's make the reasonable assumption that there's no more than 25
     * buttons. */
    const int num_max = 25;

    int buffer[num_max];
    int num = 0;

    /* We always return buttons 1 through 3. */
    buffer[num++] = 1;
    buffer[num++] = 2;
    buffer[num++] = 3;

    Binding *bind;
    TAILQ_FOREACH (bind, bindings, bindings) {
        if (num + 1 == num_max) {
            break;
        }

        /* We are only interested in whole window mouse bindings. */
        if (bind->input_type != B_MOUSE || !bind->whole_window) {
            continue;
        }

        long button;
        if (!parse_long(bind->symbol + (sizeof("button") - 1), &button, 10)) {
            ELOG("Could not parse button number, skipping this binding. Please report this bug in i3.\n");
            continue;
        }

        /* Avoid duplicates. */
        bool exists = false;
        for (int i = 0; i < num; i++) {
            if (buffer[i] == button) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            buffer[num++] = button;
        }
    }
    buffer[num++] = 0;

    int *buttons = scalloc(num, sizeof(int));
    memcpy(buttons, buffer, num * sizeof(int));

    return buttons;
}
