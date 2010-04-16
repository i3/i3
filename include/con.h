#ifndef _CON_H
#define _CON_H

Con *con_new(Con *parent);
void con_focus(Con *con);
bool con_is_leaf(Con *con);
bool con_accepts_window(Con *con);
Con *con_get_output(Con *con);
Con *con_get_workspace(Con *con);
Con *con_get_fullscreen_con(Con *con);
bool con_is_floating(Con *con);
Con *con_by_window_id(xcb_window_t window);
Con *con_by_frame_id(xcb_window_t frame);
Con *con_for_window(i3Window *window, Match **store_match);
void con_attach(Con *con, Con *parent);
void con_detach(Con *con);

enum { WINDOW_ADD = 0, WINDOW_REMOVE = 1 };
void con_fix_percent(Con *con, int action);

#endif
