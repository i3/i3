/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * startup.c: Startup notification code. Ensures a startup notification context
 *            is setup when launching applications. We store the current
 *            workspace to open windows in that startup notification context on
 *            the appropriate workspace.
 *
 */
#pragma once

#include <config.h>

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-monitor.h>

/**
 * Starts the given application by passing it through a shell. We use double
 * fork to avoid zombie processes. As the started application’s parent exits
 * (immediately), the application is reparented to init (process-id 1), which
 * correctly handles children, so we don’t have to do it :-).
 *
 * The shell used to start applications is the system's bourne shell (i.e.,
 * /bin/sh).
 *
 * The no_startup_id flag determines whether a startup notification context
 * (and ID) should be created, which is the default and encouraged behavior.
 *
 */
void start_application(const char *command, bool no_startup_id);

/**
 * Deletes a startup sequence, ignoring whether its timeout has elapsed.
 * Useful when e.g. a window is moved between workspaces and its children
 * shouldn't spawn on the original workspace.
 *
 */
void startup_sequence_delete(struct Startup_Sequence *sequence);

/**
 * Called by libstartup-notification when something happens
 *
 */
void startup_monitor_event(SnMonitorEvent *event, void *userdata);

/**
 * Renames workspaces that are mentioned in the startup sequences.
 *
 */
void startup_sequence_rename_workspace(const char *old_name, const char *new_name);

/**
 * Gets the stored startup sequence for the _NET_STARTUP_ID of a given window.
 *
 */
struct Startup_Sequence *startup_sequence_get(i3Window *cwindow,
                                              xcb_get_property_reply_t *startup_id_reply, bool ignore_mapped_leader);

/**
 * Checks if the given window belongs to a startup notification by checking if
 * the _NET_STARTUP_ID property is set on the window (or on its leader, if it’s
 * unset).
 *
 * If so, returns the workspace on which the startup was initiated.
 * Returns NULL otherwise.
 *
 */
char *startup_workspace_for_window(i3Window *cwindow, xcb_get_property_reply_t *startup_id_reply);

/**
 * Deletes the startup sequence for a window if it exists.
 *
 */
void startup_sequence_delete_by_window(i3Window *win);
