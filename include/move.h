/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * move.c: Moving containers into some direction.
 *
 */
#ifndef I3_MOVE_H
#define I3_MOVE_H

/**
 * Moves the current container in the given direction (TOK_LEFT, TOK_RIGHT,
 * TOK_UP, TOK_DOWN from cmdparse.l)
 *
 */
void tree_move(int direction);

#endif
