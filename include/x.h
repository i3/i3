/*
 * vim:ts=4:sw=4:expandtab
 */

#ifndef _X_H
#define _X_H

void x_con_init(Con *con);
void x_reinit(Con *con);
void x_con_kill(Con *con);
void x_window_kill(xcb_window_t window);
void x_draw_decoration(Con *con);
void x_push_changes(Con *con);
void x_raise_con(Con *con);

#endif
