/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2014 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * bindings.h: Functions for configuring, finding, and running bindings.
 *
 */
#pragma once

/**
 * The name of the default mode.
 *
 */
const char *DEFAULT_BINDING_MODE;

/**
 * Adds a binding from config parameters given as strings and returns a
 * pointer to the binding structure. Returns NULL if the input code could not
 * be parsed.
 *
 */
Binding *configure_binding(const char *bindtype, const char *modifiers, const char *input_code,
        const char *release, const char *command, const char *mode);

/**
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn, bool bind_mode_switch);

/**
 * Returns a pointer to the keyboard Binding with the specified modifiers and
 * keycode or NULL if no such binding exists.
 *
 */
Binding *get_keyboard_binding(uint16_t modifiers, bool key_release, xcb_keycode_t keycode);

/**
 * Translates keysymbols to keycodes for all bindings which use keysyms.
 *
 */
void translate_keysyms(void);

/**
 * Switches the key bindings to the given mode, if the mode exists
 *
 */
void switch_mode(const char *new_mode);
