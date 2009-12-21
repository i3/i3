/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <xcb/xcb.h>

#include "data.h"
#include "xinerama.h"

#ifndef _WORKSPACE_H
#define _WORKSPACE_H

/**
 * Returns a pointer to the workspace with the given number (starting at 0),
 * creating the workspace if necessary (by allocating the necessary amount of
 * memory and initializing the data structures correctly).
 *
 */
Workspace *workspace_get(int number);

/**
 * Sets the name (or just its number) for the given workspace. This has to
 * be called for every workspace as the rendering function
 * (render_internal_bar) relies on workspace->name and workspace->name_len
 * being ready-to-use.
 *
 */
void workspace_set_name(Workspace *ws, const char *name);

/**
 * Returns true if the workspace is currently visible. Especially important for
 * multi-monitor environments, as they can have multiple currenlty active
 * workspaces.
 *
 */
bool workspace_is_visible(Workspace *ws);

/** Switches to the given workspace */
void workspace_show(xcb_connection_t *conn, int workspace);

/**
 * Assigns the given workspace to the given screen by correctly updating its
 * state and reconfiguring all the clients on this workspace.
 *
 * This is called when initializing a screen and when re-assigning it to a
 * different screen which just got available (if you configured it to be on
 * screen 1 and you just plugged in screen 1).
 *
 */
void workspace_assign_to(Workspace *ws, i3Screen *screen);

/**
 * Initializes the given workspace if it is not already initialized. The given
 * screen is to be understood as a fallback, if the workspace itself either
 * was not assigned to a particular screen or cannot be placed there because
 * the screen is not attached at the moment.
 *
 */
void workspace_initialize(Workspace *ws, i3Screen *screen, bool recheck);

/**
 * Gets the first unused workspace for the given screen, taking into account
 * the preferred_screen setting of every workspace (workspace assignments).
 *
 */
Workspace *get_first_workspace_for_screen(struct screens_head *slist, i3Screen *screen);

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

/**
 * Goes through all clients on the given workspace and updates the workspace’s
 * urgent flag accordingly.
 *
 */
void workspace_update_urgent_flag(Workspace *ws);

/*
 * Returns the width of the workspace.
 *
 */
int workspace_width(Workspace *ws);

/*
 * Returns the effective height of the workspace (without the internal bar and
 * without dock clients).
 *
 */
int workspace_height(Workspace *ws);

#endif
