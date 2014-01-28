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
