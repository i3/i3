/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * util.c: Utility functions, which can be useful everywhere.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <assert.h>
#include <iconv.h>
#if defined(__OpenBSD__)
#include <sys/cdefs.h>
#endif

#include <xcb/xcb_icccm.h>

#include "i3.h"
#include "data.h"
#include "table.h"
#include "layout.h"
#include "util.h"
#include "xcb.h"
#include "client.h"

static iconv_t conversion_descriptor = 0;
struct keyvalue_table_head by_parent = TAILQ_HEAD_INITIALIZER(by_parent);
struct keyvalue_table_head by_child = TAILQ_HEAD_INITIALIZER(by_child);

int min(int a, int b) {
        return (a < b ? a : b);
}

int max(int a, int b) {
        return (a > b ? a : b);
}

/*
 * Logs the given message to stdout while prefixing the current time to it.
 * This is to be called by LOG() which includes filename/linenumber
 *
 */
void slog(char *fmt, ...) {
        va_list args;
        char timebuf[64];

        va_start(args, fmt);
        /* Get current time */
        time_t t = time(NULL);
        /* Convert time to local time (determined by the locale) */
        struct tm *tmp = localtime(&t);
        /* Generate time prefix */
        strftime(timebuf, sizeof(timebuf), "%x %X - ", tmp);
        printf("%s", timebuf);
        vprintf(fmt, args);
        va_end(args);
}

/*
 * The s* functions (safe) are wrappers around malloc, strdup, …, which exits if one of
 * the called functions returns NULL, meaning that there is no more memory available
 *
 */
void *smalloc(size_t size) {
        void *result = malloc(size);
        exit_if_null(result, "Error: out of memory (malloc(%zd))\n", size);
        return result;
}

void *scalloc(size_t size) {
        void *result = calloc(size, 1);
        exit_if_null(result, "Error: out of memory (calloc(%zd))\n", size);
        return result;
}

char *sstrdup(const char *str) {
        char *result = strdup(str);
        exit_if_null(result, "Error: out of memory (strdup())\n");
        return result;
}

/*
 * The table_* functions emulate the behaviour of libxcb-wm, which in libxcb 0.3.4 suddenly
 * vanished. Great.
 *
 */
bool table_put(struct keyvalue_table_head *head, uint32_t key, void *value) {
        struct keyvalue_element *element = scalloc(sizeof(struct keyvalue_element));
        element->key = key;
        element->value = value;

        TAILQ_INSERT_TAIL(head, element, elements);
        return true;
}

void *table_remove(struct keyvalue_table_head *head, uint32_t key) {
        struct keyvalue_element *element;

        TAILQ_FOREACH(element, head, elements)
                if (element->key == key) {
                        void *value = element->value;
                        TAILQ_REMOVE(head, element, elements);
                        free(element);
                        return value;
                }

        return NULL;
}

void *table_get(struct keyvalue_table_head *head, uint32_t key) {
        struct keyvalue_element *element;

        TAILQ_FOREACH(element, head, elements)
                if (element->key == key)
                        return element->value;

        return NULL;
}

/*
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If it
 * does not exist, /bin/sh is used.
 *
 */
void start_application(const char *command) {
        if (fork() == 0) {
                /* Child process */
                if (fork() == 0) {
                        /* Stores the path of the shell */
                        static const char *shell = NULL;

                        if (shell == NULL)
                                if ((shell = getenv("SHELL")) == NULL)
                                        shell = "/bin/sh";

                        /* This is the child */
                        execl(shell, shell, "-c", command, NULL);
                        /* not reached */
                }
                exit(0);
        }
        wait(0);
}

/*
 * Checks a generic cookie for errors and quits with the given message if there
 * was an error.
 *
 */
void check_error(xcb_connection_t *conn, xcb_void_cookie_t cookie, char *err_message) {
        xcb_generic_error_t *error = xcb_request_check(conn, cookie);
        if (error != NULL) {
                fprintf(stderr, "ERROR: %s : %d\n", err_message , error->error_code);
                xcb_disconnect(conn);
                exit(-1);
        }
}

/*
 * Converts the given string to UCS-2 big endian for use with
 * xcb_image_text_16(). The amount of real glyphs is stored in real_strlen,
 * a buffer containing the UCS-2 encoded string (16 bit per glyph) is
 * returned. It has to be freed when done.
 *
 */
char *convert_utf8_to_ucs2(char *input, int *real_strlen) {
	size_t input_size = strlen(input) + 1;
	/* UCS-2 consumes exactly two bytes for each glyph */
	int buffer_size = input_size * 2;

	char *buffer = smalloc(buffer_size);
	size_t output_size = buffer_size;
	/* We need to use an additional pointer, because iconv() modifies it */
	char *output = buffer;

	/* We convert the input into UCS-2 big endian */
        if (conversion_descriptor == 0) {
                conversion_descriptor = iconv_open("UCS-2BE", "UTF-8");
                if (conversion_descriptor == 0) {
                        fprintf(stderr, "error opening the conversion context\n");
                        exit(1);
                }
        }

	/* Get the conversion descriptor back to original state */
	iconv(conversion_descriptor, NULL, NULL, NULL, NULL);

	/* Convert our text */
	int rc = iconv(conversion_descriptor, (void*)&input, &input_size, &output, &output_size);
        if (rc == (size_t)-1) {
                perror("Converting to UCS-2 failed");
                if (real_strlen != NULL)
		        *real_strlen = 0;
                return NULL;
	}

        if (real_strlen != NULL)
	        *real_strlen = ((buffer_size - output_size) / 2) - 1;

	return buffer;
}

/*
 * Returns the client which comes next in focus stack (= was selected before) for
 * the given container, optionally excluding the given client.
 *
 */
Client *get_last_focused_client(xcb_connection_t *conn, Container *container, Client *exclude) {
        Client *current;
        SLIST_FOREACH(current, &(container->workspace->focus_stack), focus_clients)
                if ((current->container == container) && ((exclude == NULL) || (current != exclude)))
                        return current;
        return NULL;
}


/*
 * Sets the given client as focused by updating the data structures correctly,
 * updating the X input focus and finally re-decorating both windows (to signalize
 * the user the new focus situation)
 *
 */
void set_focus(xcb_connection_t *conn, Client *client, bool set_anyways) {
        /* The dock window cannot be focused, but enter notifies are still handled correctly */
        if (client->dock)
                return;

        /* Store the old client */
        Client *old_client = SLIST_FIRST(&(c_ws->focus_stack));

        /* Check if the focus needs to be changed at all */
        if (!set_anyways && (old_client == client))
                return;

        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;
        c_ws = client->workspace;
        /* Load current_col/current_row if we switch to a client without a container */
        current_col = c_ws->current_col;
        current_row = c_ws->current_row;

        /* Update container */
        if (client->container != NULL) {
                client->container->currently_focused = client;

                current_col = client->container->col;
                current_row = client->container->row;
        }

        CLIENT_LOG(client);
        /* Set focus to the entered window, and flush xcb buffer immediately */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->child, XCB_CURRENT_TIME);
        //xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, 10, 10);

        if (client->container != NULL) {
                /* Get the client which was last focused in this particular container, it may be a different
                   one than old_client */
                Client *last_focused = get_last_focused_client(conn, client->container, NULL);

                /* In stacking containers, raise the client in respect to the one which was focused before */
                if ((client->container->mode == MODE_STACK || client->container->mode == MODE_TABBED) &&
                    client->container->workspace->fullscreen_client == NULL) {
                        /* We need to get the client again, this time excluding the current client, because
                         * we might have just gone into stacking mode and need to raise */
                        Client *last_focused = get_last_focused_client(conn, client->container, client);

                        if (last_focused != NULL) {
                                LOG("raising above frame %p / child %p\n", last_focused->frame, last_focused->child);
                                uint32_t values[] = { last_focused->frame, XCB_STACK_MODE_ABOVE };
                                xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, values);
                        }
                }

                /* If it is the same one as old_client, we save us the unnecessary redecorate */
                if ((last_focused != NULL) && (last_focused != old_client))
                        redecorate_window(conn, last_focused);
        }

        /* If the last client was a floating client, we need to go to the next
         * tiling client in stack and re-decorate it. */
        if (old_client != NULL && client_is_floating(old_client)) {
                LOG("Coming from floating client, searching next tiling...\n");
                Client *current;
                SLIST_FOREACH(current, &(client->workspace->focus_stack), focus_clients) {
                        if (client_is_floating(current))
                                continue;

                        LOG("Found window: %p / child %p\n", current->frame, current->child);
                        redecorate_window(conn, current);
                        break;
                }

        }

        SLIST_REMOVE(&(client->workspace->focus_stack), client, Client, focus_clients);
        SLIST_INSERT_HEAD(&(client->workspace->focus_stack), client, focus_clients);

        /* If we’re in stacking mode, this renders the container to update changes in the title
           bars and to raise the focused client */
        if ((old_client != NULL) && (old_client != client) && !old_client->dock)
                redecorate_window(conn, old_client);

        /* redecorate_window flushes, so we don’t need to */
        redecorate_window(conn, client);
}

/*
 * Called when the user switches to another mode or when the container is
 * destroyed and thus needs to be cleaned up.
 *
 */
void leave_stack_mode(xcb_connection_t *conn, Container *container) {
        /* When going out of stacking mode, we need to close the window */
        struct Stack_Window *stack_win = &(container->stack_win);

        SLIST_REMOVE(&stack_wins, stack_win, Stack_Window, stack_windows);

        xcb_free_gc(conn, stack_win->pixmap.gc);
        xcb_free_pixmap(conn, stack_win->pixmap.id);
        xcb_destroy_window(conn, stack_win->window);

        stack_win->rect.width = -1;
        stack_win->rect.height = -1;
}

/*
 * Switches the layout of the given container taking care of the necessary house-keeping
 *
 */
void switch_layout_mode(xcb_connection_t *conn, Container *container, int mode) {
        if (mode == MODE_STACK || mode == MODE_TABBED) {
                /* When we’re already in stacking mode, nothing has to be done */
                if ((mode == MODE_STACK && container->mode == MODE_STACK) ||
                    (mode == MODE_TABBED && container->mode == MODE_TABBED))
                        return;

                if (container->mode == MODE_STACK || container->mode == MODE_TABBED)
                        goto after_stackwin;

                /* When entering stacking mode, we need to open a window on which we can draw the
                   title bars of the clients, it has height 1 because we don’t bother here with
                   calculating the correct height - it will be adjusted when rendering anyways. */
                Rect rect = {container->x, container->y, container->width, 1};

                uint32_t mask = 0;
                uint32_t values[2];

                /* Don’t generate events for our new window, it should *not* be managed */
                mask |= XCB_CW_OVERRIDE_REDIRECT;
                values[0] = 1;

                /* We want to know when… */
                mask |= XCB_CW_EVENT_MASK;
                values[1] =     XCB_EVENT_MASK_ENTER_WINDOW |   /* …mouse is moved into our window */
                                XCB_EVENT_MASK_BUTTON_PRESS |   /* …mouse is pressed */
                                XCB_EVENT_MASK_EXPOSURE;        /* …our window needs to be redrawn */

                struct Stack_Window *stack_win = &(container->stack_win);
                stack_win->window = create_window(conn, rect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, false, mask, values);

                stack_win->rect.height = 0;

                /* Initialize the entry for our cached pixmap. It will be
                 * created as soon as it’s needed (see cached_pixmap_prepare). */
                memset(&(stack_win->pixmap), 0, sizeof(struct Cached_Pixmap));
                stack_win->pixmap.referred_rect = &stack_win->rect;
                stack_win->pixmap.referred_drawable = stack_win->window;

                stack_win->container = container;

                SLIST_INSERT_HEAD(&stack_wins, stack_win, stack_windows);
        } else {
                if (container->mode == MODE_STACK || container->mode == MODE_TABBED)
                        leave_stack_mode(conn, container);
        }
after_stackwin:
        container->mode = mode;

        /* Force reconfiguration of each client */
        Client *client;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                client->force_reconfigure = true;

        render_layout(conn);

        if (container->currently_focused != NULL) {
                /* We need to make sure that this client is above *each* of the
                 * other clients in this container */
                Client *last_focused = get_last_focused_client(conn, container, container->currently_focused);

                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        if (client == container->currently_focused || client == last_focused)
                                continue;

                        LOG("setting %08x below %08x / %08x\n", client->frame, container->currently_focused->frame);
                        uint32_t values[] = { container->currently_focused->frame, XCB_STACK_MODE_BELOW };
                        xcb_configure_window(conn, client->frame,
                                             XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, values);
                }

                if (last_focused != NULL) {
                        LOG("Putting last_focused directly underneath the currently focused\n");
                        uint32_t values[] = { container->currently_focused->frame, XCB_STACK_MODE_BELOW };
                        xcb_configure_window(conn, last_focused->frame,
                                             XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, values);
                }


                set_focus(conn, container->currently_focused, true);
        }
}

/*
 * Gets the first matching client for the given window class/window title.
 * If the paramater specific is set to a specific client, only this one
 * will be checked.
 *
 */
Client *get_matching_client(xcb_connection_t *conn, const char *window_classtitle,
                            Client *specific) {
        char *to_class, *to_title, *to_title_ucs = NULL;
        int to_title_ucs_len = 0;
        Client *matching = NULL;

        to_class = sstrdup(window_classtitle);

        /* If a title was specified, split both strings at the slash */
        if ((to_title = strstr(to_class, "/")) != NULL) {
                *(to_title++) = '\0';
                /* Convert to UCS-2 */
                to_title_ucs = convert_utf8_to_ucs2(to_title, &to_title_ucs_len);
        }

        /* If we were given a specific client we only check if that one matches */
        if (specific != NULL) {
                if (client_matches_class_name(specific, to_class, to_title, to_title_ucs, to_title_ucs_len))
                        matching = specific;
                goto done;
        }

        LOG("Getting clients for class \"%s\" / title \"%s\"\n", to_class, to_title);
        for (int workspace = 0; workspace < 10; workspace++) {
                if (workspaces[workspace].screen == NULL)
                        continue;

                Client *client;
                SLIST_FOREACH(client, &(workspaces[workspace].focus_stack), focus_clients) {
                        LOG("Checking client with class=%s, name=%s\n", client->window_class, client->name);
                        if (!client_matches_class_name(client, to_class, to_title, to_title_ucs, to_title_ucs_len))
                                continue;

                        matching = client;
                        goto done;
                }
        }

done:
        free(to_class);
        FREE(to_title_ucs);
        return matching;
}

#if defined(__OpenBSD__)

/*
 * Taken from FreeBSD
 * Find the first occurrence of the byte string s in byte string l.
 *
 */
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len) {
        register char *cur, *last;
        const char *cl = (const char *)l;
        const char *cs = (const char *)s;

        /* we need something to compare */
        if (l_len == 0 || s_len == 0)
                return NULL;

        /* "s" must be smaller or equal to "l" */
        if (l_len < s_len)
                return NULL;

        /* special case where s_len == 1 */
        if (s_len == 1)
                return memchr(l, (int)*cs, l_len);

        /* the last position where its possible to find "s" in "l" */
        last = (char *)cl + l_len - s_len;

        for (cur = (char *)cl; cur <= last; cur++)
                if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
                        return cur;

        return NULL;
}

#endif

