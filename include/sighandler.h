/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 * © 2009 Jan-Erik Rediger
 *
 * sighandler.c: Interactive crash dialog upon SIGSEGV/SIGABRT/SIGFPE (offers
 *               to restart inplace).
 *
 */
#pragma once

/**
 * Setup signal handlers to safely handle SIGSEGV and SIGFPE
 *
 */
void setup_signal_handler(void);
