/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3.h: global variables that are used all over i3.
 *
 */
#ifndef _I3_H
#define _I3_H

#include <sys/time.h>
#include <sys/resource.h>

#include <xcb/xcb_keysyms.h>

#include <X11/XKBlib.h>

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-launcher.h>

#include "queue.h"
#include "data.h"
#include "xcb.h"

/** The original value of RLIMIT_CORE when i3 was started. We need to restore
 * this before starting any other process, since we set RLIMIT_CORE to
 * RLIM_INFINITY for i3 debugging versions. */
extern struct rlimit original_rlimit_core;
extern xcb_connection_t *conn;
extern int conn_screen;
/** The last timestamp we got from X11 (timestamps are included in some events
 * and are used for some things, like determining a unique ID in startup
 * notification). */
extern xcb_timestamp_t last_timestamp;
extern SnDisplay *sndisplay;
extern xcb_key_symbols_t *keysyms;
extern char **start_argv;
extern Display *xlibdpy, *xkbdpy;
extern int xkb_current_group;
extern TAILQ_HEAD(bindings_head, Binding) *bindings;
extern TAILQ_HEAD(autostarts_head, Autostart) autostarts;
extern TAILQ_HEAD(autostarts_always_head, Autostart) autostarts_always;
extern TAILQ_HEAD(ws_assignments_head, Workspace_Assignment) ws_assignments;
extern TAILQ_HEAD(assignments_head, Assignment) assignments;
extern SLIST_HEAD(stack_wins_head, Stack_Window) stack_wins;
extern xcb_screen_t *root_screen;
extern uint8_t root_depth;
extern bool xcursor_supported, xkb_supported;
extern xcb_window_t root;
extern struct ev_loop *main_loop;
extern bool only_check_config;

#endif
