#include <xcb/xcb.h>
#include <xcb/xcb_event.h>

#include <X11/XKBlib.h>

#include "queue.h"

#ifndef _I3_H
#define _I3_H

extern Display *xkbdpy;
extern TAILQ_HEAD(bindings_head, Binding) bindings;
extern xcb_event_handlers_t evenths;
extern char *pattern;
extern char **environment;
extern int num_screens;

#endif
