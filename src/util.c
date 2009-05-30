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
 * Prints the message (see printf()) to stderr, then exits the program.
 *
 */
void die(char *fmt, ...) {
        va_list args;

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);

        exit(EXIT_FAILURE);
}

/*
 * The s* functions (safe) are wrappers around malloc, strdup, …, which exits if one of
 * the called functions returns NULL, meaning that there is no more memory available
 *
 */
void *smalloc(size_t size) {
        void *result = malloc(size);
        exit_if_null(result, "Too less memory for malloc(%d)\n", size);
        return result;
}

void *scalloc(size_t size) {
        void *result = calloc(size, 1);
        exit_if_null(result, "Too less memory for calloc(%d)\n", size);
        return result;
}

char *sstrdup(const char *str) {
        char *result = strdup(str);
        exit_if_null(result, "Too less memory for strdup()\n");
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
 * Unmaps all clients (and stack windows) of the given workspace.
 *
 * This needs to be called separately when temporarily rendering
 * a workspace which is not the active workspace to force
 * reconfiguration of all clients, like in src/xinerama.c when
 * re-assigning a workspace to another screen.
 *
 */
void unmap_workspace(xcb_connection_t *conn, Workspace *u_ws) {
        Client *client;
        struct Stack_Window *stack_win;

        /* Ignore notify events because they would cause focus to be changed */
        ignore_enter_notify_forall(conn, u_ws, true);

        /* Unmap all clients of the given workspace */
        int unmapped_clients = 0;
        FOR_TABLE(u_ws)
                CIRCLEQ_FOREACH(client, &(u_ws->table[cols][rows]->clients), clients) {
                        xcb_unmap_window(conn, client->frame);
                        unmapped_clients++;
                }

        /* To find floating clients, we traverse the focus stack */
        SLIST_FOREACH(client, &(u_ws->focus_stack), focus_clients) {
                if (client->floating <= FLOATING_USER_OFF)
                        continue;

                xcb_unmap_window(conn, client->frame);
                unmapped_clients++;
        }

        /* If we did not unmap any clients, the workspace is empty and we can destroy it, at least
         * if it is not the current workspace. */
        if (unmapped_clients == 0 && u_ws != c_ws) {
                /* Re-assign the workspace of all dock clients which use this workspace */
                Client *dock;
                LOG("workspace %p is empty\n", u_ws);
                SLIST_FOREACH(dock, &(u_ws->screen->dock_clients), dock_clients) {
                        if (dock->workspace != u_ws)
                                continue;

                        LOG("Re-assigning dock client to c_ws (%p)\n", c_ws);
                        dock->workspace = c_ws;
                }
                u_ws->screen = NULL;
        }

        /* Unmap the stack windows on the given workspace, if any */
        SLIST_FOREACH(stack_win, &stack_wins, stack_windows)
                if (stack_win->container->workspace == u_ws)
                        xcb_unmap_window(conn, stack_win->window);

        ignore_enter_notify_forall(conn, u_ws, false);
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
        if (!set_anyways && (old_client == client)) {
                LOG("old_client == client, not changing focus\n");
                return;
        }

        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;
        c_ws = client->workspace;

        /* Update container */
        if (client->container != NULL) {
                client->container->currently_focused = client;

                current_col = client->container->col;
                current_row = client->container->row;
        }

        LOG("set_focus(frame %08x, child %08x, name %s)\n", client->frame, client->child, client->name);
        /* Set focus to the entered window, and flush xcb buffer immediately */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->child, XCB_CURRENT_TIME);
        //xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, 10, 10);

        if (client->container != NULL) {
                /* Get the client which was last focused in this particular container, it may be a different
                   one than old_client */
                Client *last_focused = get_last_focused_client(conn, client->container, NULL);

                /* In stacking containers, raise the client in respect to the one which was focused before */
                if (client->container->mode == MODE_STACK && client->container->workspace->fullscreen_client == NULL) {
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

        /* If we’re in stacking mode, this renders the container to update changes in the title
           bars and to raise the focused client */
        if ((old_client != NULL) && (old_client != client) && !old_client->dock)
                redecorate_window(conn, old_client);

        SLIST_REMOVE(&(client->workspace->focus_stack), client, Client, focus_clients);
        SLIST_INSERT_HEAD(&(client->workspace->focus_stack), client, focus_clients);

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

        xcb_free_gc(conn, stack_win->gc);
        xcb_destroy_window(conn, stack_win->window);

        stack_win->rect.width = -1;
        stack_win->rect.height = -1;
}

/*
 * Switches the layout of the given container taking care of the necessary house-keeping
 *
 */
void switch_layout_mode(xcb_connection_t *conn, Container *container, int mode) {
        if (mode == MODE_STACK) {
                /* When we’re already in stacking mode, nothing has to be done */
                if (container->mode == MODE_STACK)
                        return;

                /* When entering stacking mode, we need to open a window on which we can draw the
                   title bars of the clients, it has height 1 because we don’t bother here with
                   calculating the correct height - it will be adjusted when rendering anyways. */
                Rect rect = {container->x, container->y, container->width, 1 };

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
                stack_win->window = create_window(conn, rect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, mask, values);

                /* Generate a graphics context for the titlebar */
                stack_win->gc = xcb_generate_id(conn);
                xcb_create_gc(conn, stack_win->gc, stack_win->window, 0, 0);

                stack_win->container = container;

                SLIST_INSERT_HEAD(&stack_wins, stack_win, stack_windows);
        } else {
                if (container->mode == MODE_STACK)
                        leave_stack_mode(conn, container);
        }
        container->mode = mode;

        /* Force reconfiguration of each client */
        Client *client;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                client->force_reconfigure = true;

        render_layout(conn);

        if (container->currently_focused != NULL)
                set_focus(conn, container->currently_focused, true);
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
        int to_title_ucs_len;
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
