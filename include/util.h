/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * util.c: Utility functions, which can be useful everywhere within i3 (see
 *         also libi3).
 *
 */
#pragma once

#include <err.h>

#include "data.h"

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define exit_if_null(pointer, ...) \
    {                              \
        if (pointer == NULL)       \
            die(__VA_ARGS__);      \
    }
#define STARTS_WITH(string, needle) (strncasecmp(string, needle, strlen(needle)) == 0)
#define CIRCLEQ_NEXT_OR_NULL(head, elm, field) (CIRCLEQ_NEXT(elm, field) != CIRCLEQ_END(head) ? CIRCLEQ_NEXT(elm, field) : NULL)
#define CIRCLEQ_PREV_OR_NULL(head, elm, field) (CIRCLEQ_PREV(elm, field) != CIRCLEQ_END(head) ? CIRCLEQ_PREV(elm, field) : NULL)
#define FOR_TABLE(workspace)                             \
    for (int cols = 0; cols < (workspace)->cols; cols++) \
        for (int rows = 0; rows < (workspace)->rows; rows++)

#define NODES_FOREACH(head)                                                    \
    for (Con *child = (Con *)-1; (child == (Con *)-1) && ((child = 0), true);) \
    TAILQ_FOREACH(child, &((head)->nodes_head), nodes)

#define NODES_FOREACH_REVERSE(head)                                            \
    for (Con *child = (Con *)-1; (child == (Con *)-1) && ((child = 0), true);) \
    TAILQ_FOREACH_REVERSE(child, &((head)->nodes_head), nodes_head, nodes)

/* greps the ->nodes of the given head and returns the first node that matches the given condition */
#define GREP_FIRST(dest, head, condition) \
    NODES_FOREACH(head) {                 \
        if (!(condition))                 \
            continue;                     \
                                          \
        (dest) = child;                   \
        break;                            \
    }

#define FREE(pointer)          \
    do {                       \
        if (pointer != NULL) { \
            free(pointer);     \
            pointer = NULL;    \
        }                      \
    } while (0)

#define CALL(obj, member, ...) obj->member(obj, ##__VA_ARGS__)

int min(int a, int b);
int max(int a, int b);
bool rect_contains(Rect rect, uint32_t x, uint32_t y);
Rect rect_add(Rect a, Rect b);
Rect rect_sub(Rect a, Rect b);

/**
 * Returns true if the name consists of only digits.
 *
 */
__attribute__((pure)) bool name_is_digits(const char *name);

/**
 * Parses the workspace name as a number. Returns -1 if the workspace should be
 * interpreted as a "named workspace".
 *
 */
long ws_name_to_number(const char *name);

/**
 * Updates *destination with new_value and returns true if it was changed or false
 * if it was the same
 *
 */
bool update_if_necessary(uint32_t *destination, const uint32_t new_value);

/**
 * exec()s an i3 utility, for example the config file migration script or
 * i3-nagbar. This function first searches $PATH for the given utility named,
 * then falls back to the dirname() of the i3 executable path and then falls
 * back to the dirname() of the target of /proc/self/exe (on linux).
 *
 * This function should be called after fork()ing.
 *
 * The first argument of the given argv vector will be overwritten with the
 * executable name, so pass NULL.
 *
 * If the utility cannot be found in any of these locations, it exits with
 * return code 2.
 *
 */
void exec_i3_utility(char *name, char *argv[]);

/**
 * Checks a generic cookie for errors and quits with the given message if
 * there was an error.
 *
 */
void check_error(xcb_connection_t *conn, xcb_void_cookie_t cookie,
                 char *err_message);

/**
 * Checks if the given path exists by calling stat().
 *
 */
bool path_exists(const char *path);

/**
 * Restart i3 in-place
 * appends -a to argument list to disable autostart
 *
 */
void i3_restart(bool forget_layout);

#if defined(__OpenBSD__) || defined(__APPLE__)

/*
 * Taken from FreeBSD
 * Find the first occurrence of the byte string s in byte string l.
 *
 */
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);

#endif

/**
 * Starts an i3-nagbar instance with the given parameters. Takes care of
 * handling SIGCHLD and killing i3-nagbar when i3 exits.
 *
 * The resulting PID will be stored in *nagbar_pid and can be used with
 * kill_nagbar() to kill the bar later on.
 *
 */
void start_nagbar(pid_t *nagbar_pid, char *argv[]);

/**
 * Kills the i3-nagbar process, if *nagbar_pid != -1.
 *
 * If wait_for_it is set (restarting i3), this function will waitpid(),
 * otherwise, ev is assumed to handle it (reloading).
 *
 */
void kill_nagbar(pid_t *nagbar_pid, bool wait_for_it);
