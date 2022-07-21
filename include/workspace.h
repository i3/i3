/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * workspace.c: Modifying workspaces, accessing them, moving containers to
 *              workspaces.
 *
 */
#pragma once

#include <config.h>

#include "data.h"
#include "tree.h"
#include "randr.h"

/* We use NET_WM_DESKTOP_NONE for cases where we cannot determine the EWMH
 * desktop index for a window. We cannot use a negative value like -1 since we
 * need to use uint32_t as we actually need the full range of it. This is
 * technically dangerous, but it's safe to assume that we will never have more
 * than 4294967279 workspaces open at a time. */
#define NET_WM_DESKTOP_NONE 0xFFFFFFF0
#define NET_WM_DESKTOP_ALL 0xFFFFFFFF

/**
 * Stores a copy of the name of the last used workspace for the workspace
 * back-and-forth switching.
 *
 */
extern char *previous_workspace_name;

/**
 * Returns the workspace with the given name or NULL if such a workspace does
 * not exist.
 *
 */
Con *get_existing_workspace_by_name(const char *name);

/**
 * Returns the workspace with the given number or NULL if such a workspace does
 * not exist.
 *
 */
Con *get_existing_workspace_by_num(int num);

/**
 * Returns the first output that is assigned to a workspace specified by the
 * given name or number. Returns NULL if no such output exists.
 *
 * If an assignment matches by number but there is an assignment later that
 * matches by name, the second one is preferred.
 * The order of the 'ws_assignments' queue is respected: if multiple
 * assignments match the criteria, the first one is returned.
 * 'name' is ignored when NULL, 'parsed_num' is ignored when it is -1.
 *
 */
Con *get_assigned_output(const char *name, long parsed_num);

/**
 * Returns true if the first output assigned to a workspace with the given
 * workspace assignment is the same as the given output.
 *
 */
bool output_triggers_assignment(Output *output, struct Workspace_Assignment *assignment);

/**
 * Returns a pointer to the workspace with the given number (starting at 0),
 * creating the workspace if necessary (by allocating the necessary amount of
 * memory and initializing the data structures correctly).
 *
 */
Con *workspace_get(const char *num);

/**
 * Extracts workspace names from keybindings (e.g. “web” from “bindsym $mod+1
 * workspace web”), so that when an output needs a workspace, i3 can start with
 * the first configured one. Needs to be called before reorder_bindings() so
 * that the config-file order is used, not the i3-internal order.
 *
 */
void extract_workspace_names_from_bindings(void);

/**
 * Returns a pointer to a new workspace in the given output. The workspace
 * is created attached to the tree hierarchy through the given content
 * container.
 *
 */
Con *create_workspace_on_output(Output *output, Con *content);

/**
 * Returns true if the workspace is currently visible. Especially important for
 * multi-monitor environments, as they can have multiple currently active
 * workspaces.
 *
 */
bool workspace_is_visible(Con *ws);

/**
 * Switches to the given workspace
 *
 */
void workspace_show(Con *ws);

/**
 * Looks up the workspace by name and switches to it.
 *
 */
void workspace_show_by_name(const char *num);

/**
 * Returns the next workspace.
 *
 */
Con *workspace_next(void);

/**
 * Returns the previous workspace.
 *
 */
Con *workspace_prev(void);

/**
 * Returns the next workspace on the same output
 *
 */
Con *workspace_next_on_output(void);

/**
 * Returns the previous workspace on the same output
 *
 */
Con *workspace_prev_on_output(void);

/**
 * Focuses the previously focused workspace.
 *
 */
void workspace_back_and_forth(void);

/**
 * Returns the previously focused workspace con, or NULL if unavailable.
 *
 */
Con *workspace_back_and_forth_get(void);

/**
 * Goes through all clients on the given workspace and updates the workspace’s
 * urgent flag accordingly.
 *
 */
void workspace_update_urgent_flag(Con *ws);

/**
 * 'Forces' workspace orientation by moving all cons into a new split-con with
 * the same orientation as the workspace and then changing the workspace
 * orientation.
 *
 */
void ws_force_orientation(Con *ws, orientation_t orientation);

/**
 * Called when a new con (with a window, not an empty or split con) should be
 * attached to the workspace (for example when managing a new window or when
 * moving an existing window to the workspace level).
 *
 * Depending on the workspace_layout setting, this function either returns the
 * workspace itself (default layout) or creates a new stacked/tabbed con and
 * returns that.
 *
 */
Con *workspace_attach_to(Con *ws);

/**
 * Creates a new container and re-parents all of children from the given
 * workspace into it.
 *
 * The container inherits the layout from the workspace.
 */
Con *workspace_encapsulate(Con *ws);

/**
 * Move the given workspace to the specified output.
 *
 */
void workspace_move_to_output(Con *ws, Output *output);
