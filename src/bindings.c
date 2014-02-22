/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2014 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * bindings.c: Functions for configuring, finding and, running bindings.
 */
#include "all.h"

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
        const char *release, const char *command, const char *modename) {
    Binding *new_binding = scalloc(sizeof(Binding));
    DLOG("bindtype %s, modifiers %s, input code %s, release %s\n", bindtype, modifiers, input_code, release);
    new_binding->release = (release != NULL ? B_UPON_KEYRELEASE : B_UPON_KEYPRESS);
    if (strcmp(bindtype, "bindsym") == 0) {
        new_binding->symbol = sstrdup(input_code);
    } else {
        // TODO: strtol with proper error handling
        new_binding->keycode = atoi(input_code);
        if (new_binding->keycode == 0) {
            ELOG("Could not parse \"%s\" as an input code, ignoring this binding.\n", input_code);
            FREE(new_binding);
            return NULL;
        }
    }
    new_binding->mods = modifiers_from_str(modifiers);
    new_binding->command = sstrdup(command);
    new_binding->input_type = B_KEYBOARD;

    struct Mode *mode = mode_from_name(modename);
    TAILQ_INSERT_TAIL(mode->bindings, new_binding, bindings);

    return new_binding;
}

static void grab_keycode_for_binding(xcb_connection_t *conn, Binding *bind, uint32_t keycode) {
    if (bind->input_type != B_KEYBOARD)
        return;

    DLOG("Grabbing %d with modifiers %d (with mod_mask_lock %d)\n", keycode, bind->mods, bind->mods | XCB_MOD_MASK_LOCK);
    /* Grab the key in all combinations */
    #define GRAB_KEY(modifier) \
        do { \
            xcb_grab_key(conn, 0, root, modifier, keycode, \
                         XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC); \
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
 * Returns a pointer to the keyboard Binding with the specified modifiers and
 * keycode or NULL if no such binding exists.
 *
 */
Binding *get_keyboard_binding(uint16_t modifiers, bool key_release, xcb_keycode_t keycode) {
    Binding *bind;

    if (!key_release) {
        /* On a KeyPress event, we first reset all
         * B_UPON_KEYRELEASE_IGNORE_MODS bindings back to B_UPON_KEYRELEASE */
        TAILQ_FOREACH(bind, bindings, bindings) {
            if (bind->input_type != B_KEYBOARD)
                continue;
            if (bind->release == B_UPON_KEYRELEASE_IGNORE_MODS)
                bind->release = B_UPON_KEYRELEASE;
        }
    }

    TAILQ_FOREACH(bind, bindings, bindings) {
        /* First compare the modifiers (unless this is a
         * B_UPON_KEYRELEASE_IGNORE_MODS binding and this is a KeyRelease
         * event) */
        if (bind->input_type != B_KEYBOARD)
            continue;
        if (bind->mods != modifiers &&
            (bind->release != B_UPON_KEYRELEASE_IGNORE_MODS ||
             !key_release))
            continue;

        /* If a symbol was specified by the user, we need to look in
         * the array of translated keycodes for the event’s keycode */
        if (bind->symbol != NULL) {
            if (memmem(bind->translated_to,
                       bind->number_keycodes * sizeof(xcb_keycode_t),
                       &keycode, sizeof(xcb_keycode_t)) == NULL)
                continue;
        } else {
            /* This case is easier: The user specified a keycode */
            if (bind->keycode != keycode)
                continue;
        }

        /* If this keybinding is a KeyRelease binding, it matches the key which
         * the user pressed. We therefore mark it as
         * B_UPON_KEYRELEASE_IGNORE_MODS for later, so that the user can
         * release the modifiers before the actual key and the KeyRelease will
         * still be matched. */
        if (bind->release == B_UPON_KEYRELEASE && !key_release)
            bind->release = B_UPON_KEYRELEASE_IGNORE_MODS;

        /* Check if the binding is for a KeyPress or a KeyRelease event */
        if ((bind->release == B_UPON_KEYPRESS && key_release) ||
            (bind->release >= B_UPON_KEYRELEASE && !key_release))
            continue;

        break;
    }

    return (bind == TAILQ_END(bindings) ? NULL : bind);
}
