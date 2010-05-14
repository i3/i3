/*
 * vim:ts=4:sw=4:expandtab
 */

#ifndef _TREE_H
#define _TREE_H

extern Con *croot;
/* TODO: i am not sure yet how much access to the focused container should
 * be permitted to source files */
extern Con *focused;
TAILQ_HEAD(all_cons_head, Con);
extern struct all_cons_head all_cons;

void tree_init();
Con *tree_open_con(Con *con);
void tree_split(Con *con, orientation_t orientation);
void level_up();
void level_down();
void tree_render();
void tree_close_con();
void tree_next(char way, orientation_t orientation);
void tree_move(char way, orientation_t orientation);
void tree_close(Con *con, bool kill_window);
bool tree_restore();

#endif
