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
#include <err.h>

#include "data.h"

#ifndef _UTIL_H
#define _UTIL_H

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define exit_if_null(pointer, ...) { if (pointer == NULL) die(__VA_ARGS__); }
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)
#define CIRCLEQ_NEXT_OR_NULL(head, elm, field) (CIRCLEQ_NEXT(elm, field) != CIRCLEQ_END(head) ? \
                                                CIRCLEQ_NEXT(elm, field) : NULL)
#define CIRCLEQ_PREV_OR_NULL(head, elm, field) (CIRCLEQ_PREV(elm, field) != CIRCLEQ_END(head) ? \
                                                CIRCLEQ_PREV(elm, field) : NULL)
#define FOR_TABLE(workspace) \
                        for (int cols = 0; cols < (workspace)->cols; cols++) \
                                for (int rows = 0; rows < (workspace)->rows; rows++)
#define FREE(pointer) do { \
        if (pointer != NULL) { \
                free(pointer); \
                pointer = NULL; \
        } \
} \
while (0)

/** ##__VA_ARGS__ means: leave out __VA_ARGS__ completely if it is empty, that
   is, delete the preceding comma */
#define LOG(fmt, ...) slog("%s:%s:%d - " fmt, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

TAILQ_HEAD(keyvalue_table_head, keyvalue_element);
extern struct keyvalue_table_head by_parent;
extern struct keyvalue_table_head by_child;

int min(int a, int b);
int max(int a, int b);

/**
 * Logs the given message to stdout while prefixing the current time to it.
 * This is to be called by LOG() which includes filename/linenumber
 *
 */
void slog(char *fmt, ...);

/**
 * Safe-wrapper around malloc which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
void *smalloc(size_t size);

/**
 * Safe-wrapper around calloc which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
void *scalloc(size_t size);

/**
 * Safe-wrapper around strdup which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
char *sstrdup(const char *str);

/**
 * Inserts an element into the given keyvalue-table using the given key.
 *
 */
bool table_put(struct keyvalue_table_head *head, uint32_t key, void *value);

/**
 * Removes the element from the given keyvalue-table with the given key and
 * returns its value;
 *
 */
void *table_remove(struct keyvalue_table_head *head, uint32_t key);

/**
 * Returns the value of the element of the given keyvalue-table with the given
 * key.
 *
 */
void *table_get(struct keyvalue_table_head *head, uint32_t key);

/**
 * Starts the given application by passing it through a shell. We use double
 * fork to avoid zombie processes. As the started application’s parent exits
 * (immediately), the application is reparented to init (process-id 1), which
 * correctly handles childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If
 * it does not exist, /bin/sh is used.
 *
 */
void start_application(const char *command);

/**
 * Checks a generic cookie for errors and quits with the given message if
 * there was an error.
 *
 */
void check_error(xcb_connection_t *conn, xcb_void_cookie_t cookie,
                 char *err_message);

/**
 * Converts the given string to UCS-2 big endian for use with
 * xcb_image_text_16(). The amount of real glyphs is stored in real_strlen, a
 * buffer containing the UCS-2 encoded string (16 bit per glyph) is
 * returned. It has to be freed when done.
 *
 */
char *convert_utf8_to_ucs2(char *input, int *real_strlen);

/**
 * Returns the client which comes next in focus stack (= was selected before) for
 * the given container, optionally excluding the given client.
 *
 */
Client *get_last_focused_client(xcb_connection_t *conn, Container *container,
                                Client *exclude);

/**
 * Unmaps all clients (and stack windows) of the given workspace.
 *
 * This needs to be called separately when temporarily rendering a workspace
 * which is not the active workspace to force reconfiguration of all clients,
 * like in src/xinerama.c when re-assigning a workspace to another screen.
 *
 */
void unmap_workspace(xcb_connection_t *conn, Workspace *u_ws);

/**
 * Unmaps all clients (and stack windows) of the given workspace.
 *
 * This needs to be called separately when temporarily rendering
 * a workspace which is not the active workspace to force
 * reconfiguration of all clients, like in src/xinerama.c when
 * re-assigning a workspace to another screen.
 *
 */
void unmap_workspace(xcb_connection_t *conn, Workspace *u_ws);

/**
 * Sets the given client as focused by updating the data structures correctly,
 * updating the X input focus and finally re-decorating both windows (to
 * signalize the user the new focus situation)
 *
 */
void set_focus(xcb_connection_t *conn, Client *client, bool set_anyways);

/**
 * Called when the user switches to another mode or when the container is
 * destroyed and thus needs to be cleaned up.
 *
 */
void leave_stack_mode(xcb_connection_t *conn, Container *container);

/**
 * Switches the layout of the given container taking care of the necessary
 * house-keeping
 *
 */
void switch_layout_mode(xcb_connection_t *conn, Container *container, int mode);

/**
 * Gets the first matching client for the given window class/window title.
 * If the paramater specific is set to a specific client, only this one
 * will be checked.
 *
 */
Client *get_matching_client(xcb_connection_t *conn,
                            const char *window_classtitle, Client *specific);

#endif
