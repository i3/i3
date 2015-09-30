/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * move.c: Moving containers into some direction.
 *
 */
#pragma once

/**
 * Moves the given container in the given direction (TOK_LEFT, TOK_RIGHT,
 * TOK_UP, TOK_DOWN from cmdparse.l)
 *
 */
void tree_move(Con *con, int direction);
