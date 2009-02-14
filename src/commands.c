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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <xcb/xcb.h>

#include "util.h"
#include "data.h"
#include "table.h"
#include "layout.h"
#include "i3.h"

static bool focus_window_in_container(xcb_connection_t *connection, Container *container,
                direction_t direction) {
        /* If this container is empty, we’re done */
        if (container->currently_focused == NULL)
                return false;

        Client *candidad;
        if (direction == D_UP)
                candidad = CIRCLEQ_PREV(container->currently_focused, clients);
        else if (direction == D_DOWN)
                candidad = CIRCLEQ_NEXT(container->currently_focused, clients);

        /* If we don’t have anything to select, we’re done */
        if (candidad == CIRCLEQ_END(&(container->clients)))
                return false;

        /* Set focus if we could successfully move */
        container->currently_focused = candidad;
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE, candidad->child, XCB_CURRENT_TIME);
        render_layout(connection);

        return true;
}

static void focus_window(xcb_connection_t *connection, direction_t direction) {
        printf("focusing direction %d\n", direction);
        /* TODO: for horizontal default layout, this has to be expanded to LEFT/RIGHT */
        if (direction == D_UP || direction == D_DOWN) {
                /* Let’s see if we can perform up/down focus in the current container */
                Container *container = CUR_CELL;

                /* There always is a container. If not, current_col or current_row is wrong */
                assert(container != NULL);

                if (focus_window_in_container(connection, container, direction))
                        return;
        } else if (direction == D_LEFT || direction == D_RIGHT) {
                if (direction == D_RIGHT && cell_exists(current_col+1, current_row))
                        current_col++;
                else if (direction == D_LEFT && cell_exists(current_col-1, current_row))
                        current_col--;
                else {
                        printf("nah, not possible\n");
                        return;
                }
                if (CUR_CELL->currently_focused != NULL) {
                        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_NONE,
                                        CUR_CELL->currently_focused->child, XCB_CURRENT_TIME);
                        render_layout(connection);
                }

        } else {
                printf("direction unhandled\n");
        }
}

/*
 * Tries to move the window inside its current container.
 *
 * Returns true if the window could be moved, false otherwise.
 *
 */
static bool move_current_window_in_container(xcb_connection_t *connection, Client *client,
                direction_t direction) {
        Client *other = (direction == D_UP ? CIRCLEQ_PREV(client, clients) :
                                                CIRCLEQ_NEXT(client, clients));

        if (other == CIRCLEQ_END(&(client->container->clients)))
                return false;

        printf("i can do that\n");
        /* We can move the client inside its current container */
        CIRCLEQ_REMOVE(&(client->container->clients), client, clients);
        if (direction == D_UP)
                CIRCLEQ_INSERT_BEFORE(&(client->container->clients), other, client, clients);
        else CIRCLEQ_INSERT_AFTER(&(client->container->clients), other, client, clients);
        render_layout(connection);
        return true;
}

/*
 * Moves the current window to the given direction, creating a column/row if
 * necessary
 *
 */
static void move_current_window(xcb_connection_t *connection, direction_t direction) {
        printf("moving window to direction %d\n", direction);
        /* Get current window */
        Container *container = CUR_CELL,
                  *new;

        /* There has to be a container, see focus_window() */
        assert(container != NULL);

        /* If there is no window, we’re done */
        if (container->currently_focused == NULL)
                return;

        /* As soon as the client is moved away, the next client in the old
         * container needs to get focus, if any. Therefore, we save it here. */
        Client *current_client = container->currently_focused;
        Client *to_focus = CIRCLEQ_NEXT(current_client, clients);
        if (to_focus == CIRCLEQ_END(&(container->clients)))
                to_focus = NULL;

        switch (direction) {
                case D_LEFT:
                        if (current_col == 0)
                                return;

                        new = CUR_TABLE[--current_col][current_row];
                        break;
                case D_RIGHT:
                        if (current_col == (c_ws->cols-1))
                                expand_table_cols(c_ws);

                        new = CUR_TABLE[++current_col][current_row];
                        break;
                case D_UP:
                        /* TODO: if we’re at the up-most position, move the rest of the table down */
                        if (move_current_window_in_container(connection, current_client, D_UP) ||
                                current_row == 0)
                                return;

                        new = CUR_TABLE[current_col][--current_row];
                        break;
                case D_DOWN:
                        if (move_current_window_in_container(connection, current_client, D_DOWN))
                                return;

                        if (current_row == (c_ws->rows-1))
                                expand_table_rows(c_ws);

                        new = CUR_TABLE[current_col][++current_row];
                        break;
        }

        /* Remove it from the old container and put it into the new one */
        CIRCLEQ_REMOVE(&(container->clients), current_client, clients);
        CIRCLEQ_INSERT_TAIL(&(new->clients), current_client, clients);

        /* Update data structures */
        current_client->container = new;
        container->currently_focused = to_focus;
        new->currently_focused = current_client;

        /* TODO: delete all empty columns/rows */

        render_layout(connection);
}

/*
 * "Snaps" the current container (not possible for windows, because it works at table base)
 * to the given direction, that is, adjusts cellspan/rowspan
 *
 */
static void snap_current_container(xcb_connection_t *connection, direction_t direction) {
        printf("snapping container to direction %d\n", direction);

        Container *container = CUR_CELL;
        int i;

        assert(container != NULL);

        switch (direction) {
                case D_LEFT:
                        /* Snap to the left is actually a move to the left and then a snap right */
                        move_current_window(connection, D_LEFT);
                        snap_current_container(connection, D_RIGHT);
                        return;
                case D_RIGHT:
                        /* Check if the cell is used */
                        if (!cell_exists(container->col + 1, container->row) ||
                                CUR_TABLE[container->col+1][container->row]->currently_focused != NULL) {
                                printf("cannot snap to right - the cell is already used\n");
                                return;
                        }

                        /* Check if there are other cells with rowspan, which are in our way.
                         * If so, reduce their rowspan. */
                        for (i = container->row-1; i >= 0; i--) {
                                printf("we got cell %d, %d with rowspan %d\n",
                                                container->col+1, i, CUR_TABLE[container->col+1][i]->rowspan);
                                while ((CUR_TABLE[container->col+1][i]->rowspan-1) >= (container->row - i))
                                        CUR_TABLE[container->col+1][i]->rowspan--;
                                printf("new rowspan = %d\n", CUR_TABLE[container->col+1][i]->rowspan);
                        }

                        container->colspan++;
                        break;
                case D_UP:
                        move_current_window(connection, D_UP);
                        snap_current_container(connection, D_DOWN);
                        return;
                case D_DOWN:
                        printf("snapping down\n");
                        if (!cell_exists(container->col, container->row+1) ||
                                CUR_TABLE[container->col][container->row+1]->currently_focused != NULL) {
                                printf("cannot snap down - the cell is already used\n");
                                return;
                        }

                        for (i = container->col-1; i >= 0; i--) {
                                printf("we got cell %d, %d with colspan %d\n",
                                                i, container->row+1, CUR_TABLE[i][container->row+1]->colspan);
                                while ((CUR_TABLE[i][container->row+1]->colspan-1) >= (container->col - i))
                                        CUR_TABLE[i][container->row+1]->colspan--;
                                printf("new colspan = %d\n", CUR_TABLE[i][container->row+1]->colspan);

                        }

                        container->rowspan++;
                        break;
        }

        render_layout(connection);
}

static void show_workspace(xcb_connection_t *conn, int workspace) {
        int cols, rows;
        Client *client;
        printf("show_workspace(%d)\n", workspace);

        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;

        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;

        /* TODO: does grabbing the server actually bring us any (speed)advantages? */
        //xcb_grab_server(conn);

        /* Unmap all clients */
        for (cols = 0; cols < c_ws->cols; cols++)
                for (rows = 0; rows < c_ws->rows; rows++) {
                        CIRCLEQ_FOREACH(client, &(c_ws->table[cols][rows]->clients), clients)
                                xcb_unmap_window(conn, client->frame);
                }

        c_ws = &workspaces[workspace-1];
        current_row = c_ws->current_row;
        current_col = c_ws->current_col;
        printf("new current row = %d, current col = %d\n", current_row, current_col);

        /* Map all clients on the new workspace */
        for (cols = 0; cols < c_ws->cols; cols++)
                for (rows = 0; rows < c_ws->rows; rows++) {
                        CIRCLEQ_FOREACH(client, &(c_ws->table[cols][rows]->clients), clients)
                                xcb_map_window(conn, client->frame);
                }

        /* Restore focus on the new workspace */
        if (CUR_CELL->currently_focused != NULL)
                xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, CUR_CELL->currently_focused->child, XCB_CURRENT_TIME);
        else xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, root, XCB_CURRENT_TIME);

        //xcb_ungrab_server(conn);

        render_layout(conn);
}

/*
 * Parses a command, see file CMDMODE for more information
 *
 */
void parse_command(xcb_connection_t *conn, const char *command) {
        printf("--- parsing command \"%s\" ---\n", command);
        /* Hmm, just to be sure */
        if (command[0] == '\0')
                return;

        /* Is it an <exec>? */
        if (strncmp(command, "exec ", strlen("exec ")) == 0) {
                printf("starting \"%s\"\n", command + strlen("exec "));
                start_application(command+strlen("exec "));
                return;
        }

        /* Is it 'f' for fullscreen? */
        if (command[0] == 'f') {
                if (CUR_CELL->currently_focused == NULL)
                        return;
                toggle_fullscreen(conn, CUR_CELL->currently_focused);
                return;
        }

        /* Is it a <with>? */
        if (command[0] == 'w') {
                /* TODO: implement */
                printf("not yet implemented.\n");
                return;
        }

        /* It's a normal <cmd> */
        int times;
        char *rest = NULL;
        enum { ACTION_FOCUS, ACTION_MOVE, ACTION_SNAP } action = ACTION_FOCUS;
        direction_t direction;
        times = strtol(command, &rest, 10);
        if (rest == NULL) {
                printf("Invalid command (\"%s\")\n", command);
                return;
        }
        if (*rest == 'm' || *rest == 's') {
                action = (*rest == 'm' ? ACTION_MOVE : ACTION_SNAP);
                rest++;
        }

        if (*rest == '\0') {
                /* No rest? This was a tag number, not a times specification */
                show_workspace(conn, times);
                return;
        }

        /* Now perform action to <where> */
        while (*rest != '\0') {
                if (*rest == 'h')
                        direction = D_LEFT;
                else if (*rest == 'j')
                        direction = D_DOWN;
                else if (*rest == 'k')
                        direction = D_UP;
                else if (*rest == 'l')
                        direction = D_RIGHT;
                else {
                        printf("unknown direction: %c\n", *rest);
                        return;
                }

                if (action == ACTION_FOCUS)
                        focus_window(conn, direction);
                else if (action == ACTION_MOVE)
                        move_current_window(conn, direction);
                else if (action == ACTION_SNAP)
                        snap_current_container(conn, direction);

                rest++;
        }

        printf("--- done ---\n");
}
