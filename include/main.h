/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2013 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * main.c: Initialization, main loop
 *
 */
#ifndef I3_MAIN_H
#define I3_MAIN_H

/**
 * Enable or disable the main X11 event handling function.
 * This is used by drag_pointer() which has its own, modal event handler, which
 * takes precedence over the normal event handler.
 *
 */
void main_set_x11_cb(bool enable);

#endif
