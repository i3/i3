/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * workspace.c: Modifying workspaces, accessing them, moving containers to
 *              workspaces.
 *
 */
#pragma once

#include "data.h"
#include "tree.h"
#include "randr.h"

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

/*
 * Returns a pointer to a new workspace in the given output. The workspace
 * is created attached to the tree hierarchy through the given content
 * container.
 *
 */
Con *create_workspace_on_output(Output *output, Con *content);

#if 0
/**
 * Sets the name (or just its number) for the given workspace. This has to
 * be called for every workspace as the rendering function
 * (render_internal_bar) relies on workspace->name and workspace->name_len
 * being ready-to-use.
 *
 */
void workspace_set_name(Workspace *ws, const char *name);
#endif

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
