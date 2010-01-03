/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 * © 2009-2010 Jan-Erik Rediger
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _SIGHANDLER_H
#define _SIGHANDLER_H

/**
 * Setup signal handlers to safely handle SIGSEGV and SIGFPE
 *
 */
void setup_signal_handler();

#endif
