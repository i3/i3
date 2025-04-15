/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * output.c: Output (monitor) related functions.
 *
 */
#pragma once

#include <config.h>

/**
 * Returns the output container below the given output container.
 *
 */
Con *output_get_content(Con *output);

/**
 * Returns an 'output' corresponding to one of left/right/down/up or a specific
 * output name.
 *
 */
Output *get_output_from_string(Output *current_output, const char *output_str);

/**
 * Retrieves the primary name of an output.
 *
 */
char *output_primary_name(Output *output);

/**
 * Retrieves the output for a given container. Never returns NULL.
 * There is an assertion that _will_ fail if the container is inside an
 * internal workspace. Use con_is_internal() if needed before calling this
 * function.
 */
Output *get_output_for_con(Con *con);

/**
 * Iterates over all outputs and pushes sticky windows to the currently visible
 * workspace on that output.
 *
 * old_focus is used to determine if a sticky window is going to be focused.
 * old_focus might be different than the currently focused container because the
 * caller might need to temporarily change the focus and then call
 * output_push_sticky_windows. For example, workspace_show needs to set focus to
 * one of its descendants first, then call output_push_sticky_windows that
 * should focus a sticky window if it was the focused in the previous workspace.
 *
 */
void output_push_sticky_windows(Con *old_focus);
