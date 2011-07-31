/*
 * vim:ts=4:sw=4:expandtab
 */

#ifndef _MOVE_H
#define _MOVE_H

/**
 * Moves the current container in the given direction (TOK_LEFT, TOK_RIGHT,
 * TOK_UP, TOK_DOWN from cmdparse.l)
 *
 */
void tree_move(int direction);

#endif
