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

#include <config.h>

/**
 * Moves the given container in the given direction (TOK_LEFT, TOK_RIGHT,
 * TOK_UP, TOK_DOWN from cmdparse.l)
 *
 */
void tree_move(Con *con, int direction);

typedef enum { BEFORE,
               AFTER } position_t;

/**
 * This function detaches 'con' from its parent and inserts it either before or
 * after 'target'.
 *
 */
void insert_con_into(Con *con, Con *target, position_t position);
