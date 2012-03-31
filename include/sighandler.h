/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 * © 2009-2010 Jan-Erik Rediger
 *
 * sighandler.c: Interactive crash dialog upon SIGSEGV/SIGABRT/SIGFPE (offers
 *               to restart inplace).
 *
 */
#ifndef _SIGHANDLER_H
#define _SIGHANDLER_H

/**
 * Setup signal handlers to safely handle SIGSEGV and SIGFPE
 *
 */
void setup_signal_handler(void);

#endif
