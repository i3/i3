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
 * Initializes the given workspace if it is not already initialized. The given
 * screen is to be understood as a fallback, if the workspace itself either
 * was not assigned to a particular screen or cannot be placed there because
 * the screen is not attached at the moment.
 *
 */
void workspace_initialize(Workspace *ws, i3Screen *screen);

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


void workspace_map_clients(xcb_connection_t *conn, Workspace *ws);

#endif
