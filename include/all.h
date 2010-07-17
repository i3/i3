/*
 * This header file includes all relevant files of i3 and the most often used
 * system header files. This reduces boilerplate (the amount of code duplicated
 * at the beginning of each source file) and is not significantly slower at
 * compile-time.
 *
 */
#ifndef _ALL_H
#define _ALL_H

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

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>

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

#endif
