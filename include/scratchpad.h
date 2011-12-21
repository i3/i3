/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * scratchpad.c: Scratchpad functions (TODO: more description)
 *
 */
#ifndef _SCRATCHPAD_H
#define _SCRATCHPAD_H

/**
 * Moves the specified window to the __i3_scratch workspace, making it floating
 * and setting the appropriate scratchpad_state.
 *
 * Gets called upon the command 'move scratchpad'.
 *
 */
void scratchpad_move(Con *con);

/**
 * Either shows the top-most scratchpad window (con == NULL) or shows the
 * specified con (if it is scratchpad window).
 *
 * When called with con == NULL and the currently focused window is a
 * scratchpad window, this serves as a shortcut to hide it again (so the user
 * can press the same key to quickly look something up).
 *
 */
void scratchpad_show(Con *con);

#endif
