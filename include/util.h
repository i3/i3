/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
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

int min(int a, int b);
int max(int a, int b);
bool rect_contains(Rect rect, uint32_t x, uint32_t y);

/**
 * Updates *destination with new_value and returns true if it was changed or false
 * if it was the same
 *
 */
bool update_if_necessary(uint32_t *destination, const uint32_t new_value);

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
 * Safe-wrapper around realloc which exits if realloc returns NULL (meaning
 * that there is no more memory available).
 *
 */
void *srealloc(void *ptr, size_t size);

/**
 * Safe-wrapper around strdup which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
char *sstrdup(const char *str);

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

#if 0
/**
 * Returns the client which comes next in focus stack (= was selected before) for
 * the given container, optionally excluding the given client.
 *
 */
Client *get_last_focused_client(xcb_connection_t *conn, Container *container,
                                Client *exclude);
#endif

#if 0
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

/*
 * Restart i3 in-place
 * appends -a to argument list to disable autostart
 *
 */
void i3_restart();

#if defined(__OpenBSD__)
/* OpenBSD does not provide memmem(), so we provide FreeBSD’s implementation */
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);
#endif

#endif
