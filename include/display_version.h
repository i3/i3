/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * display_version.c: displays the running i3 version, runs as part of
 *                    i3 --moreversion.
 */
#pragma once

#include <config.h>

/**
 * Connects to i3 to find out the currently running version. Useful since it
 * might be different from the version compiled into this binary (maybe the
 * user didn’t correctly install i3 or forgot to restart it).
 *
 * The output looks like this:
 * Running i3 version: 4.2-202-gb8e782c (2012-08-12, branch "next") (pid 14804)
 *
 * The i3 binary you just called: /home/michael/i3/i3
 * The i3 binary you are running: /home/michael/i3/i3
 *
 */
void display_running_version(void);
