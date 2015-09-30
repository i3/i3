/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * This header file includes all relevant files of i3 and the most often used
 * system header files. This reduces boilerplate (the amount of code duplicated
 * at the beginning of each source file) and is not significantly slower at
 * compile-time.
 *
 */
#ifndef I3_ALL_H
#define I3_ALL_H

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glob.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>

#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>

/* Contains compatibility definitions for old libxcb versions */
#ifdef XCB_COMPAT
#include "xcb_compat.h"
#endif

#include "data.h"
#include "util.h"
#include "ipc.h"
#include "tree.h"
#include "log.h"
#include "xcb.h"
#include "manage.h"
#include "workspace.h"
#include "i3.h"
#include "x.h"
#include "click.h"
#include "key_press.h"
#include "floating.h"
#include "config.h"
#include "handlers.h"
#include "randr.h"
#include "xinerama.h"
#include "con.h"
#include "load_layout.h"
#include "render.h"
#include "window.h"
#include "match.h"
#include "cmdparse.h"
#include "xcursor.h"
#include "resize.h"
#include "sighandler.h"
#include "move.h"
#include "output.h"
#include "ewmh.h"
#include "assignments.h"
#include "regex.h"
#include "libi3.h"
#include "startup.h"
#include "scratchpad.h"
#include "commands.h"
#include "commands_parser.h"
#include "bindings.h"
#include "config_directives.h"
#include "config_parser.h"
#include "fake_outputs.h"
#include "display_version.h"
#include "restore_layout.h"
#include "main.h"

#endif
