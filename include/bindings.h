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

extern pid_t command_error_nagbar_pid;

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
 * Returns a pointer to the Binding that matches the given xcb event or NULL if
 * no such binding exists.
 *
 */
Binding *get_binding_from_xcb_event(xcb_generic_event_t *event);

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

/**
 * Checks for duplicate key bindings (the same keycode or keysym is configured
 * more than once). If a duplicate binding is found, a message is printed to
 * stderr and the has_errors variable is set to true, which will start
 * i3-nagbar.
 *
 */
void check_for_duplicate_bindings(struct context *context);

/**
 * Frees the binding. If bind is null, it simply returns.
 */
void binding_free(Binding *bind);

/**
 * Runs the given binding and handles parse errors. If con is passed, it will
 * execute the command binding with that container selected by criteria.
 * Returns a CommandResult for running the binding's command. Caller should
 * render tree if needs_tree_render is true. Free with command_result_free().
 *
 */
CommandResult *run_binding(Binding *bind, Con *con);
