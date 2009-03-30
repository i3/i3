/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <xcb/xcb.h>

#include "data.h"

#ifndef _UTIL_H
#define _UTIL_H

#define exit_if_null(pointer, ...) { if (pointer == NULL) die(__VA_ARGS__); }
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)
#define CIRCLEQ_NEXT_OR_NULL(head, elm, field) (CIRCLEQ_NEXT(elm, field) != CIRCLEQ_END(head) ? \
                                                CIRCLEQ_NEXT(elm, field) : NULL)
#define CIRCLEQ_PREV_OR_NULL(head, elm, field) (CIRCLEQ_PREV(elm, field) != CIRCLEQ_END(head) ? \
                                                CIRCLEQ_PREV(elm, field) : NULL)
#define FOR_TABLE(workspace) \
                        for (int cols = 0; cols < workspace->cols; cols++) \
                                for (int rows = 0; rows < workspace->rows; rows++)
#define FREE(pointer) do { \
        if (pointer == NULL) { \
                free(pointer); \
                pointer = NULL; \
        } \
} \
while (0)

/* ##__VA_ARGS__ means: leave out __VA_ARGS__ completely if it is empty, that is,
   delete the preceding comma */
#define LOG(fmt, ...) slog("%s:%s:%d - " fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)


int min(int a, int b);
int max(int a, int b);
void slog(char *fmt, ...);
void die(char *fmt, ...);
void *smalloc(size_t size);
void *scalloc(size_t size);
char *sstrdup(const char *str);
void start_application(const char *command);
void check_error(xcb_connection_t *conn, xcb_void_cookie_t cookie, char *err_message);
char *convert_utf8_to_ucs2(char *input, int *real_strlen);
void remove_client_from_container(xcb_connection_t *conn, Client *client, Container *container);
void set_focus(xcb_connection_t *conn, Client *client);
void leave_stack_mode(xcb_connection_t *conn, Container *container);
void switch_layout_mode(xcb_connection_t *conn, Container *container, int mode);
void warp_pointer_into(xcb_connection_t *conn, Client *client);
void toggle_fullscreen(xcb_connection_t *conn, Client *client);
void kill_window(xcb_connection_t *conn, Client *window);

#endif
