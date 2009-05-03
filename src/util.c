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
		*real_strlen = 0;
                return NULL;
	}

	*real_strlen = ((buffer_size - output_size) / 2) - 1;

	return buffer;
}

/*
 * Removes the given client from the container, either because it will be inserted into another
 * one or because it was unmapped
 *
 */
void remove_client_from_container(xcb_connection_t *conn, Client *client, Container *container) {
        CIRCLEQ_REMOVE(&(container->clients), client, clients);

        SLIST_REMOVE(&(container->workspace->focus_stack), client, Client, focus_clients);

        /* If the container will be empty now and is in stacking mode, we need to
           unmap the stack_win */
        if (CIRCLEQ_EMPTY(&(container->clients)) && container->mode == MODE_STACK) {
                struct Stack_Window *stack_win = &(container->stack_win);
                stack_win->rect.height = 0;
                xcb_unmap_window(conn, stack_win->window);
        }
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
        Client *old_client = CUR_CELL->currently_focused;

        /* Check if the focus needs to be changed at all */
        if (!set_anyways && (old_client == client)) {
                LOG("old_client == client, not changing focus\n");
                return;
        }

        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;
        c_ws = client->container->workspace;

        /* Update container */
        client->container->currently_focused = client;

        current_col = client->container->col;
        current_row = client->container->row;

        LOG("set_focus(frame %08x, child %08x, name %s)\n", client->frame, client->child, client->name);
        /* Set focus to the entered window, and flush xcb buffer immediately */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->child, XCB_CURRENT_TIME);
        //xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, 10, 10);

        /* Get the client which was last focused in this particular container, it may be a different
           one than old_client */
        Client *last_focused = get_last_focused_client(conn, client->container, NULL);

        /* In stacking containers, raise the client in respect to the one which was focused before */
        if (client->container->mode == MODE_STACK) {
                /* We need to get the client again, this time excluding the current client, because
                 * we might have just gone into stacking mode and need to raise */
                Client *last_focused = get_last_focused_client(conn, client->container, client);

                LOG("raising above frame %p / child %p\n", last_focused->frame, last_focused->child);
                uint32_t values[] = { last_focused->frame, XCB_STACK_MODE_ABOVE };
                xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, values);
        }

        /* If it is the same one as old_client, we save us the unnecessary redecorate */
        if ((last_focused != NULL) && (last_focused != old_client))
                redecorate_window(conn, last_focused);

        /* If we’re in stacking mode, this renders the container to update changes in the title
           bars and to raise the focused client */
        if ((old_client != NULL) && (old_client != client) && !old_client->dock)
                redecorate_window(conn, old_client);

        SLIST_REMOVE(&(client->container->workspace->focus_stack), client, Client, focus_clients);
        SLIST_INSERT_HEAD(&(client->container->workspace->focus_stack), client, focus_clients);

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
 * Warps the pointer into the given client (in the middle of it, to be specific), therefore
 * selecting it
 *
 */
void warp_pointer_into(xcb_connection_t *conn, Client *client) {
        int mid_x = client->rect.width / 2,
            mid_y = client->rect.height / 2;
        xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, mid_x, mid_y);
}

/*
 * Toggles fullscreen mode for the given client. It updates the data structures and
 * reconfigures (= resizes/moves) the client and its frame to the full size of the
 * screen. When leaving fullscreen, re-rendering the layout is forced.
 *
 */
void toggle_fullscreen(xcb_connection_t *conn, Client *client) {
        /* clients without a container (docks) cannot be focused */
        assert(client->container != NULL);

        Workspace *workspace = client->container->workspace;

        if (!client->fullscreen) {
                if (workspace->fullscreen_client != NULL) {
                        LOG("Not entering fullscreen mode, there already is a fullscreen client.\n");
                        return;
                }
                client->fullscreen = true;
                workspace->fullscreen_client = client;
                LOG("Entering fullscreen mode...\n");
                /* We just entered fullscreen mode, let’s configure the window */
                 uint32_t mask = XCB_CONFIG_WINDOW_X |
                                 XCB_CONFIG_WINDOW_Y |
                                 XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT;
                uint32_t values[4] = {workspace->rect.x,
                                      workspace->rect.y,
                                      workspace->rect.width,
                                      workspace->rect.height};

                LOG("child itself will be at %dx%d with size %dx%d\n",
                                values[0], values[1], values[2], values[3]);

                xcb_configure_window(conn, client->frame, mask, values);

                /* Child’s coordinates are relative to the parent (=frame) */
                values[0] = 0;
                values[1] = 0;
                xcb_configure_window(conn, client->child, mask, values);

                /* Raise the window */
                values[0] = XCB_STACK_MODE_ABOVE;
                xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_STACK_MODE, values);

                Rect child_rect = workspace->rect;
                child_rect.x = child_rect.y = 0;
                fake_configure_notify(conn, child_rect, client->child);
        } else {
                LOG("leaving fullscreen mode\n");
                client->fullscreen = false;
                workspace->fullscreen_client = NULL;
                /* Because the coordinates of the window haven’t changed, it would not be
                   re-configured if we don’t set the following flag */
                client->force_reconfigure = true;
                /* We left fullscreen mode, redraw the whole layout to ensure enternotify events are disabled */
                render_layout(conn);
        }

        xcb_flush(conn);
}

/*
 * Returns true if the client supports the given protocol atom (like WM_DELETE_WINDOW)
 *
 */
static bool client_supports_protocol(xcb_connection_t *conn, Client *client, xcb_atom_t atom) {
        xcb_get_property_cookie_t cookie;
        xcb_get_wm_protocols_reply_t protocols;
        bool result = false;

        cookie = xcb_get_wm_protocols_unchecked(conn, client->child, atoms[WM_PROTOCOLS]);
        if (xcb_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1)
                return false;

        /* Check if the client’s protocols have the requested atom set */
        for (uint32_t i = 0; i < protocols.atoms_len; i++)
                if (protocols.atoms[i] == atom)
                        result = true;

        xcb_get_wm_protocols_reply_wipe(&protocols);

        return result;
}

/*
 * Kills the given window using WM_DELETE_WINDOW or xcb_kill_window
 *
 */
void kill_window(xcb_connection_t *conn, Client *window) {
        /* If the client does not support WM_DELETE_WINDOW, we kill it the hard way */
        if (!client_supports_protocol(conn, window, atoms[WM_DELETE_WINDOW])) {
                LOG("Killing window the hard way\n");
                xcb_kill_client(conn, window->child);
                return;
        }

        xcb_client_message_event_t ev;

        memset(&ev, 0, sizeof(xcb_client_message_event_t));

        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.window = window->child;
        ev.type = atoms[WM_PROTOCOLS];
        ev.format = 32;
        ev.data.data32[0] = atoms[WM_DELETE_WINDOW];
        ev.data.data32[1] = XCB_CURRENT_TIME;

        LOG("Sending WM_DELETE to the client\n");
        xcb_send_event(conn, false, window->child, XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
        xcb_flush(conn);
}
