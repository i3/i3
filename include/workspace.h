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
 * If created is not NULL, *created will be set to whether or not the
 * workspace has just been created.
 *
 */
Con *workspace_get(const char *num, bool *created);

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
 * multi-monitor environments, as they can have multiple currenlty active
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

#if 0
/**
 * Assigns the given workspace to the given screen by correctly updating its
 * state and reconfiguring all the clients on this workspace.
 *
 * This is called when initializing a screen and when re-assigning it to a
 * different screen which just got available (if you configured it to be on
 * screen 1 and you just plugged in screen 1).
 *
 */
void workspace_assign_to(Workspace *ws, Output *screen, bool hide_it);

/**
 * Initializes the given workspace if it is not already initialized. The given
 * screen is to be understood as a fallback, if the workspace itself either
 * was not assigned to a particular screen or cannot be placed there because
 * the screen is not attached at the moment.
 *
 */
void workspace_initialize(Workspace *ws, Output *screen, bool recheck);

/**
 * Gets the first unused workspace for the given screen, taking into account
 * the preferred_screen setting of every workspace (workspace assignments).
 *
 */
Workspace *get_first_workspace_for_output(Output *screen);

/**
 * Unmaps all clients (and stack windows) of the given workspace.
 *
 * This needs to be called separately when temporarily rendering a workspace
 * which is not the active workspace to force reconfiguration of all clients,
 * like in src/xinerama.c when re-assigning a workspace to another screen.
 *
 */
void workspace_unmap_clients(xcb_connection_t *conn, Workspace *u_ws);

/**
 * Maps all clients (and stack windows) of the given workspace.
 *
 */
void workspace_map_clients(xcb_connection_t *conn, Workspace *ws);
#endif

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
