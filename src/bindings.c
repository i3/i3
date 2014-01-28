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
