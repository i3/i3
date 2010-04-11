/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
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
#include "randr.h"
#include "client.h"
#include "floating.h"
#include "xcb.h"
#include "config.h"
#include "workspace.h"
#include "commands.h"
#include "resize.h"
#include "log.h"
#include "sighandler.h"
#include "manage.h"
#include "ipc.h"

bool focus_window_in_container(xcb_connection_t *conn, Container *container, direction_t direction) {
        /* If this container is empty, we’re done */
        if (container->currently_focused == NULL)
                return false;

        /* Get the previous/next client or wrap around */
        Client *candidate = NULL;
        if (direction == D_UP) {
                if ((candidate = CIRCLEQ_PREV_OR_NULL(&(container->clients), container->currently_focused, clients)) == NULL)
                        candidate = CIRCLEQ_LAST(&(container->clients));
        }
        else if (direction == D_DOWN) {
                if ((candidate = CIRCLEQ_NEXT_OR_NULL(&(container->clients), container->currently_focused, clients)) == NULL)
                        candidate = CIRCLEQ_FIRST(&(container->clients));
        } else ELOG("Direction not implemented!\n");

        /* If we could not switch, the container contains exactly one client. We return false */
        if (candidate == container->currently_focused)
                return false;

        /* Set focus */
        set_focus(conn, candidate, true);

        return true;
}

typedef enum { THING_WINDOW, THING_CONTAINER, THING_SCREEN } thing_t;

static void jump_to_mark(xcb_connection_t *conn, const char *mark) {
        Client *current;
        LOG("Jumping to \"%s\"\n", mark);

        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces)
                SLIST_FOREACH(current, &(ws->focus_stack), focus_clients) {
                        if (current->mark == NULL || strcmp(current->mark, mark) != 0)
                                continue;

                        set_focus(conn, current, true);
                        workspace_show(conn, current->workspace->num + 1);
                        return;
                }

        ELOG("No window with this mark found\n");
}

static void focus_thing(xcb_connection_t *conn, direction_t direction, thing_t thing) {
        DLOG("focusing direction %d\n", direction);

        int new_row = current_row,
            new_col = current_col;
        Container *container = CUR_CELL;
        Workspace *t_ws = c_ws;

        /* Makes sure new_col and new_row are within bounds of the new workspace */
#define CHECK_COLROW_BOUNDARIES \
        do { \
                if (new_col >= t_ws->cols) \
                        new_col = (t_ws->cols - 1); \
                if (new_row >= t_ws->rows) \
                        new_row = (t_ws->rows - 1); \
        } while (0)

        /* There always is a container. If not, current_col or current_row is wrong */
        assert(container != NULL);

        if (container->workspace->fullscreen_client != NULL) {
                LOG("You're in fullscreen mode. Forcing focus to operate on whole screens\n");
                thing = THING_SCREEN;
        }

        /* For focusing screens, situation is different: we get the rect
         * of the current screen, then get the screen which is on its
         * right/left/bottom/top and just switch to the workspace on
         * the target screen. */
        if (thing == THING_SCREEN) {
                Output *cs = c_ws->output;
                assert(cs != NULL);
                Rect bounds = cs->rect;

                if (direction == D_RIGHT)
                        bounds.x += bounds.width;
                else if (direction == D_LEFT)
                        bounds.x -= bounds.width;
                else if (direction == D_UP)
                        bounds.y -= bounds.height;
                else bounds.y += bounds.height;

                Output *target = get_output_containing(bounds.x, bounds.y);
                if (target == NULL) {
                        DLOG("Target output NULL\n");
                        /* Wrap around if the target screen is out of bounds */
                        if (direction == D_RIGHT)
                                target = get_output_most(D_LEFT, cs);
                        else if (direction == D_LEFT)
                                target = get_output_most(D_RIGHT, cs);
                        else if (direction == D_UP)
                                target = get_output_most(D_DOWN, cs);
                        else target = get_output_most(D_UP, cs);
                }

                DLOG("Switching to ws %d\n", target->current_workspace + 1);
                workspace_show(conn, target->current_workspace->num + 1);
                return;
        }

        /* TODO: for horizontal default layout, this has to be expanded to LEFT/RIGHT */
        if (direction == D_UP || direction == D_DOWN) {
                if (thing == THING_WINDOW)
                        /* Let’s see if we can perform up/down focus in the current container */
                        if (focus_window_in_container(conn, container, direction))
                                return;

                if (direction == D_DOWN && cell_exists(t_ws, current_col, current_row+1))
                        new_row = current_row + t_ws->table[current_col][current_row]->rowspan;
                else if (direction == D_UP && cell_exists(c_ws, current_col, current_row-1)) {
                        /* Set new_row as a sane default, but it may get overwritten in a second */
                        new_row--;

                        /* Search from the top to correctly handle rowspanned containers */
                        for (int rows = 0; rows < current_row; rows += t_ws->table[current_col][rows]->rowspan) {
                                if (new_row > (rows + (t_ws->table[current_col][rows]->rowspan - 1)))
                                        continue;

                                new_row = rows;
                                break;
                        }
                } else {
                        /* Let’s see if there is a screen down/up there to which we can switch */
                        DLOG("container is at %d with height %d\n", container->y, container->height);
                        Output *output;
                        int destination_y = (direction == D_UP ? (container->y - 1) : (container->y + container->height + 1));
                        if ((output = get_output_containing(container->x, destination_y)) == NULL) {
                                DLOG("Wrapping screen around vertically\n");
                                /* No screen found? Then wrap */
                                output = get_output_most((direction == D_UP ? D_DOWN : D_UP), container->workspace->output);
                        }
                        t_ws = output->current_workspace;
                        new_row = (direction == D_UP ? (t_ws->rows - 1) : 0);
                }

                CHECK_COLROW_BOUNDARIES;

                DLOG("new_col = %d, new_row = %d\n", new_col, new_row);
                if (t_ws->table[new_col][new_row]->currently_focused == NULL) {
                        DLOG("Cell empty, checking for colspanned client above...\n");
                        for (int cols = 0; cols < new_col; cols += t_ws->table[cols][new_row]->colspan) {
                                if (new_col > (cols + (t_ws->table[cols][new_row]->colspan - 1)))
                                        continue;

                                new_col = cols;
                                DLOG("Fixed it to new col %d\n", new_col);
                                break;
                        }
                }

                if (t_ws->table[new_col][new_row]->currently_focused == NULL) {
                        DLOG("Cell still empty, checking for full cols above spanned width...\n");
                        DLOG("new_col = %d\n", new_col);
                        DLOG("colspan = %d\n", container->colspan);
                        for (int cols = new_col;
                             cols < container->col + container->colspan;
                             cols += t_ws->table[cols][new_row]->colspan) {
                                DLOG("candidate: new_row = %d, cols = %d\n", new_row, cols);
                                if (t_ws->table[cols][new_row]->currently_focused == NULL)
                                        continue;

                                new_col = cols;
                                DLOG("Fixed it to new col %d\n", new_col);
                                break;
                        }
                }
        } else if (direction == D_LEFT || direction == D_RIGHT) {
                if (direction == D_RIGHT && cell_exists(t_ws, current_col+1, current_row))
                        new_col = current_col + t_ws->table[current_col][current_row]->colspan;
                else if (direction == D_LEFT && cell_exists(t_ws, current_col-1, current_row)) {
                        /* Set new_col as a sane default, but it may get overwritten in a second */
                        new_col--;

                        /* Search from the left to correctly handle colspanned containers */
                        for (int cols = 0; cols < current_col; cols += t_ws->table[cols][current_row]->colspan) {
                                if (new_col > (cols + (t_ws->table[cols][current_row]->colspan - 1)))
                                        continue;

                                new_col = cols;
                                break;
                        }
                } else {
                        /* Let’s see if there is a screen left/right here to which we can switch */
                        DLOG("container is at %d with width %d\n", container->x, container->width);
                        Output *output;
                        int destination_x = (direction == D_LEFT ? (container->x - 1) : (container->x + container->width + 1));
                        if ((output = get_output_containing(destination_x, container->y)) == NULL) {
                                DLOG("Wrapping screen around horizontally\n");
                                output = get_output_most((direction == D_LEFT ? D_RIGHT : D_LEFT), container->workspace->output);
                        }
                        t_ws = output->current_workspace;
                        new_col = (direction == D_LEFT ? (t_ws->cols - 1) : 0);
                }

                CHECK_COLROW_BOUNDARIES;

                DLOG("new_col = %d, new_row = %d\n", new_col, new_row);
                if (t_ws->table[new_col][new_row]->currently_focused == NULL) {
                        DLOG("Cell empty, checking for rowspanned client above...\n");
                        for (int rows = 0; rows < new_row; rows += t_ws->table[new_col][rows]->rowspan) {
                                if (new_row > (rows + (t_ws->table[new_col][rows]->rowspan - 1)))
                                        continue;

                                new_row = rows;
                                DLOG("Fixed it to new row %d\n", new_row);
                                break;
                        }
                }

                if (t_ws->table[new_col][new_row]->currently_focused == NULL) {
                        DLOG("Cell still empty, checking for full cols near full spanned height...\n");
                        DLOG("new_row = %d\n", new_row);
                        DLOG("rowspan = %d\n", container->rowspan);
                        for (int rows = new_row;
                             rows < container->row + container->rowspan;
                             rows += t_ws->table[new_col][rows]->rowspan) {
                                DLOG("candidate: new_col = %d, rows = %d\n", new_col, rows);
                                if (t_ws->table[new_col][rows]->currently_focused == NULL)
                                        continue;

                                new_row = rows;
                                DLOG("Fixed it to new col %d\n", new_row);
                                break;
                        }
                }

        } else {
                ELOG("direction unhandled\n");
                return;
        }

        CHECK_COLROW_BOUNDARIES;

        if (t_ws->table[new_col][new_row]->currently_focused != NULL)
                set_focus(conn, t_ws->table[new_col][new_row]->currently_focused, true);
}

/*
 * Tries to move the window inside its current container.
 *
 * Returns true if the window could be moved, false otherwise.
 *
 */
static bool move_current_window_in_container(xcb_connection_t *conn, Client *client,
                direction_t direction) {
        assert(client->container != NULL);

        Client *other = (direction == D_UP ? CIRCLEQ_PREV(client, clients) :
                                             CIRCLEQ_NEXT(client, clients));

        if (other == CIRCLEQ_END(&(client->container->clients)))
                return false;

        DLOG("i can do that\n");
        /* We can move the client inside its current container */
        CIRCLEQ_REMOVE(&(client->container->clients), client, clients);
        if (direction == D_UP)
                CIRCLEQ_INSERT_BEFORE(&(client->container->clients), other, client, clients);
        else CIRCLEQ_INSERT_AFTER(&(client->container->clients), other, client, clients);
        render_layout(conn);
        return true;
}

/*
 * Moves the current window or whole container to the given direction, creating a column/row if
 * necessary.
 *
 */
static void move_current_window(xcb_connection_t *conn, direction_t direction) {
        LOG("moving window to direction %s\n", (direction == D_UP ? "up" : (direction == D_DOWN ? "down" :
                                            (direction == D_LEFT ? "left" : "right"))));
        /* Get current window */
        Container *container = CUR_CELL,
                  *new = NULL;

        /* There has to be a container, see focus_window() */
        assert(container != NULL);

        /* If there is no window or the dock window is focused, we’re done */
        if (container->currently_focused == NULL ||
            container->currently_focused->dock)
                return;

        /* As soon as the client is moved away, the last focused client in the old
         * container needs to get focus, if any. Therefore, we save it here. */
        Client *current_client = container->currently_focused;
        Client *to_focus = get_last_focused_client(conn, container, current_client);

        if (to_focus == NULL) {
                to_focus = CIRCLEQ_NEXT_OR_NULL(&(container->clients), current_client, clients);
                if (to_focus == NULL)
                        to_focus = CIRCLEQ_PREV_OR_NULL(&(container->clients), current_client, clients);
        }

        switch (direction) {
                case D_LEFT:
                        /* If we’re at the left-most position, move the rest of the table right */
                        if (current_col == 0) {
                                expand_table_cols_at_head(c_ws);
                                new = CUR_CELL;
                        } else
                                new = CUR_TABLE[--current_col][current_row];
                        break;
                case D_RIGHT:
                        if (current_col == (c_ws->cols-1))
                                expand_table_cols(c_ws);

                        new = CUR_TABLE[++current_col][current_row];
                        break;
                case D_UP:
                        if (move_current_window_in_container(conn, current_client, D_UP))
                                return;

                        /* if we’re at the up-most position, move the rest of the table down */
                        if (current_row == 0) {
                                expand_table_rows_at_head(c_ws);
                                new = CUR_CELL;
                        } else
                                new = CUR_TABLE[current_col][--current_row];
                        break;
                case D_DOWN:
                        if (move_current_window_in_container(conn, current_client, D_DOWN))
                                return;

                        if (current_row == (c_ws->rows-1))
                                expand_table_rows(c_ws);

                        new = CUR_TABLE[current_col][++current_row];
                        break;
                /* To make static analyzers happy: */
                default:
                        return;
        }

        /* Remove it from the old container and put it into the new one */
        client_remove_from_container(conn, current_client, container, true);

        if (new->currently_focused != NULL)
                CIRCLEQ_INSERT_AFTER(&(new->clients), new->currently_focused, current_client, clients);
        else CIRCLEQ_INSERT_TAIL(&(new->clients), current_client, clients);
        SLIST_INSERT_HEAD(&(new->workspace->focus_stack), current_client, focus_clients);

        /* Update data structures */
        current_client->container = new;
        current_client->workspace = new->workspace;
        container->currently_focused = to_focus;
        new->currently_focused = current_client;

        Workspace *workspace = container->workspace;

        /* delete all empty columns/rows */
        cleanup_table(conn, workspace);

        /* Fix colspan/rowspan if it’d overlap */
        fix_colrowspan(conn, workspace);

        render_workspace(conn, workspace->output, workspace);
        xcb_flush(conn);

        set_focus(conn, current_client, true);
}

static void move_current_container(xcb_connection_t *conn, direction_t direction) {
        LOG("moving container to direction %s\n", (direction == D_UP ? "up" : (direction == D_DOWN ? "down" :
                                            (direction == D_LEFT ? "left" : "right"))));
        /* Get current window */
        Container *container = CUR_CELL,
                  *new = NULL;

        Container **old = &CUR_CELL;

        /* There has to be a container, see focus_window() */
        assert(container != NULL);

        switch (direction) {
                case D_LEFT:
                        /* If we’re at the left-most position, move the rest of the table right */
                        if (current_col == 0) {
                                expand_table_cols_at_head(c_ws);
                                new = CUR_CELL;
                                old = &CUR_TABLE[current_col+1][current_row];
                        } else
                                new = CUR_TABLE[--current_col][current_row];
                        break;
                case D_RIGHT:
                        if (current_col == (c_ws->cols-1))
                                expand_table_cols(c_ws);

                        new = CUR_TABLE[++current_col][current_row];
                        break;
                case D_UP:
                        /* if we’re at the up-most position, move the rest of the table down */
                        if (current_row == 0) {
                                expand_table_rows_at_head(c_ws);
                                new = CUR_CELL;
                                old = &CUR_TABLE[current_col][current_row+1];
                        } else
                                new = CUR_TABLE[current_col][--current_row];
                        break;
                case D_DOWN:
                        if (current_row == (c_ws->rows-1))
                                expand_table_rows(c_ws);

                        new = CUR_TABLE[current_col][++current_row];
                        break;
                /* To make static analyzers happy: */
                default:
                        return;
        }

        DLOG("old = %d,%d and new = %d,%d\n", container->col, container->row, new->col, new->row);

        /* Swap the containers */
        int col = new->col;
        int row = new->row;

        *old = new;
        new->col = container->col;
        new->row = container->row;

        CUR_CELL = container;
        container->col = col;
        container->row = row;

        Workspace *workspace = container->workspace;

        /* delete all empty columns/rows */
        cleanup_table(conn, workspace);

        /* Fix colspan/rowspan if it’d overlap */
        fix_colrowspan(conn, workspace);

        render_layout(conn);
}

/*
 * "Snaps" the current container (not possible for windows, because it works at table base)
 * to the given direction, that is, adjusts cellspan/rowspan
 *
 */
static void snap_current_container(xcb_connection_t *conn, direction_t direction) {
        LOG("snapping container to direction %d\n", direction);

        Container *container = CUR_CELL;

        assert(container != NULL);

        switch (direction) {
                case D_LEFT:
                        /* Snap to the left is actually a move to the left and then a snap right */
                        if (!cell_exists(container->workspace, container->col - 1, container->row) ||
                            CUR_TABLE[container->col-1][container->row]->currently_focused != NULL) {
                                ELOG("cannot snap to left - the cell is already used\n");
                                return;
                        }

                        move_current_window(conn, D_LEFT);
                        snap_current_container(conn, D_RIGHT);
                        return;
                case D_RIGHT: {
                        /* Check if the cell is used */
                        int new_col = container->col + container->colspan;
                        for (int i = 0; i < container->rowspan; i++)
                                if (!cell_exists(container->workspace, new_col, container->row + i) ||
                                    CUR_TABLE[new_col][container->row + i]->currently_focused != NULL) {
                                        ELOG("cannot snap to right - the cell is already used\n");
                                        return;
                                }

                        /* Check if there are other cells with rowspan, which are in our way.
                         * If so, reduce their rowspan. */
                        for (int i = container->row-1; i >= 0; i--) {
                                DLOG("we got cell %d, %d with rowspan %d\n",
                                                new_col, i, CUR_TABLE[new_col][i]->rowspan);
                                while ((CUR_TABLE[new_col][i]->rowspan-1) >= (container->row - i))
                                        CUR_TABLE[new_col][i]->rowspan--;
                                DLOG("new rowspan = %d\n", CUR_TABLE[new_col][i]->rowspan);
                        }

                        container->colspan++;
                        break;
                }
                case D_UP:
                        if (!cell_exists(container->workspace, container->col, container->row - 1) ||
                            CUR_TABLE[container->col][container->row-1]->currently_focused != NULL) {
                                ELOG("cannot snap to top - the cell is already used\n");
                                return;
                        }

                        move_current_window(conn, D_UP);
                        snap_current_container(conn, D_DOWN);
                        return;
                case D_DOWN: {
                        DLOG("snapping down\n");
                        int new_row = container->row + container->rowspan;
                        for (int i = 0; i < container->colspan; i++)
                                if (!cell_exists(container->workspace, container->col + i, new_row) ||
                                    CUR_TABLE[container->col + i][new_row]->currently_focused != NULL) {
                                        ELOG("cannot snap down - the cell is already used\n");
                                        return;
                                }

                        for (int i = container->col-1; i >= 0; i--) {
                                DLOG("we got cell %d, %d with colspan %d\n",
                                                i, new_row, CUR_TABLE[i][new_row]->colspan);
                                while ((CUR_TABLE[i][new_row]->colspan-1) >= (container->col - i))
                                        CUR_TABLE[i][new_row]->colspan--;
                                DLOG("new colspan = %d\n", CUR_TABLE[i][new_row]->colspan);

                        }

                        container->rowspan++;
                        break;
                }
                /* To make static analyzers happy: */
                default:
                        return;
        }

        render_layout(conn);
}

static void move_floating_window_to_workspace(xcb_connection_t *conn, Client *client, int workspace) {
        /* t_ws (to workspace) is just a container pointer to the workspace we’re switching to */
        Workspace *t_ws = workspace_get(workspace-1),
                  *old_ws = client->workspace;

        LOG("moving floating\n");

        workspace_initialize(t_ws, c_ws->output, false);

        /* Check if there is already a fullscreen client on the destination workspace and
         * stop moving if so. */
        if (client->fullscreen && (t_ws->fullscreen_client != NULL)) {
                ELOG("Not moving: Fullscreen client already existing on destination workspace.\n");
                return;
        }

        floating_assign_to_workspace(client, t_ws);

        /* If we’re moving it to an invisible screen, we need to unmap it */
        if (!workspace_is_visible(t_ws)) {
                DLOG("This workspace is not visible, unmapping\n");
                client_unmap(conn, client);
        } else {
                /* If this is not the case, we move the window to a workspace
                 * which is on another screen, so we also need to adjust its
                 * coordinates. */
                DLOG("before x = %d, y = %d\n", client->rect.x, client->rect.y);
                uint32_t relative_x = client->rect.x - old_ws->rect.x,
                         relative_y = client->rect.y - old_ws->rect.y;
                DLOG("rel_x = %d, rel_y = %d\n", relative_x, relative_y);
                if (client->fullscreen) {
                        client_enter_fullscreen(conn, client, false);
                        memcpy(&(client->rect), &(t_ws->rect), sizeof(Rect));
                } else {
                        client->rect.x = t_ws->rect.x + relative_x;
                        client->rect.y = t_ws->rect.y + relative_y;
                        DLOG("after x = %d, y = %d\n", client->rect.x, client->rect.y);
                        reposition_client(conn, client);
                        xcb_flush(conn);
                }
        }

        /* Configure the window above all tiling windows (or below a fullscreen
         * window, if any) */
        if (t_ws->fullscreen_client != NULL) {
                uint32_t values[] = { t_ws->fullscreen_client->frame, XCB_STACK_MODE_BELOW };
                xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, values);
        } else {
                Client *last_tiling;
                SLIST_FOREACH(last_tiling, &(t_ws->focus_stack), focus_clients)
                        if (!client_is_floating(last_tiling))
                                break;
                if (last_tiling != SLIST_END(&(t_ws->focus_stack))) {
                        uint32_t values[] = { last_tiling->frame, XCB_STACK_MODE_ABOVE };
                        xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, values);
                }
        }

        DLOG("done\n");

        render_layout(conn);

        if (workspace_is_visible(t_ws)) {
                client_warp_pointer_into(conn, client);
                set_focus(conn, client, true);
        }
}

/*
 * Moves the currently selected window to the given workspace
 *
 */
static void move_current_window_to_workspace(xcb_connection_t *conn, int workspace) {
        LOG("Moving current window to workspace %d\n", workspace);

        Container *container = CUR_CELL;

        assert(container != NULL);

        /* t_ws (to workspace) is just a container pointer to the workspace we’re switching to */
        Workspace *t_ws = workspace_get(workspace-1);

        Client *current_client = container->currently_focused;
        if (current_client == NULL) {
                ELOG("No currently focused client in current container.\n");
                return;
        }
        Client *to_focus = CIRCLEQ_NEXT_OR_NULL(&(container->clients), current_client, clients);
        if (to_focus == NULL)
                to_focus = CIRCLEQ_PREV_OR_NULL(&(container->clients), current_client, clients);

        workspace_initialize(t_ws, container->workspace->output, false);
        /* Check if there is already a fullscreen client on the destination workspace and
         * stop moving if so. */
        if (current_client->fullscreen && (t_ws->fullscreen_client != NULL)) {
                ELOG("Not moving: Fullscreen client already existing on destination workspace.\n");
                return;
        }

        Container *to_container = t_ws->table[t_ws->current_col][t_ws->current_row];

        assert(to_container != NULL);

        client_remove_from_container(conn, current_client, container, true);
        if (container->workspace->fullscreen_client == current_client)
                container->workspace->fullscreen_client = NULL;

        /* TODO: insert it to the correct position */
        CIRCLEQ_INSERT_TAIL(&(to_container->clients), current_client, clients);

        SLIST_INSERT_HEAD(&(to_container->workspace->focus_stack), current_client, focus_clients);
        DLOG("Moved.\n");

        current_client->container = to_container;
        current_client->workspace = to_container->workspace;
        container->currently_focused = to_focus;
        to_container->currently_focused = current_client;

        /* If we’re moving it to an invisible screen, we need to unmap it */
        if (!workspace_is_visible(to_container->workspace)) {
                DLOG("This workspace is not visible, unmapping\n");
                client_unmap(conn, current_client);
        } else {
                if (current_client->fullscreen) {
                        DLOG("Calling client_enter_fullscreen again\n");
                        client_enter_fullscreen(conn, current_client, false);
                }
        }

        /* delete all empty columns/rows */
        cleanup_table(conn, container->workspace);

        render_layout(conn);

        if (workspace_is_visible(to_container->workspace)) {
                client_warp_pointer_into(conn, current_client);
                set_focus(conn, current_client, true);
        }
}

/*
 * Brings the given window class / title to the current workspace.
 *
 */
static void bring_window_here(xcb_connection_t *conn, const char *arguments) {
	char *classtitle;
	Client *client;

	/* The first character is a quote, this was checked before */
	classtitle = sstrdup(arguments+1);
	/* The last character is a quote, we just set it to NULL */
	classtitle[strlen(classtitle)-1] = '\0';

	if ((client = get_matching_client(conn, classtitle, NULL)) == NULL) {
		free(classtitle);
		ELOG("No matching client found.\n");
		return;
	}

	/* This is the workspace num that we are on */
	int current_workspace_num = c_ws->output->current_workspace->num + 1;
	/* This is the workspace num that the client is on */
	int clients_workspace_num = client->workspace->num + 1;

	free(classtitle);
	workspace_show(conn, clients_workspace_num);
	set_focus(conn, client, true);
	if (client_is_floating(client))
		move_floating_window_to_workspace(conn, client, current_workspace_num);
	else move_current_window_to_workspace(conn, current_workspace_num);
	workspace_show(conn, current_workspace_num);
	set_focus(conn, client, true);

}

/*
 * Jumps to the given window class / title.
 * Title is matched using strstr, that is, matches if it appears anywhere
 * in the string. Regular expressions seem to be a bit overkill here. However,
 * if we need them for something else somewhen, we may introduce them here, too.
 *
 */
static void jump_to_window(xcb_connection_t *conn, const char *arguments) {
        char *classtitle;
        Client *client;

        /* The first character is a quote, this was checked before */
        classtitle = sstrdup(arguments+1);
        /* The last character is a quote, we just set it to NULL */
        classtitle[strlen(classtitle)-1] = '\0';

        if ((client = get_matching_client(conn, classtitle, NULL)) == NULL) {
                free(classtitle);
                ELOG("No matching client found.\n");
                return;
        }

        free(classtitle);
        workspace_show(conn, client->workspace->num + 1);
        set_focus(conn, client, true);
}

/*
 * Jump directly to the specified workspace, row and col.
 * Great for reaching windows that you always keep in the same spot (hello irssi, I'm looking at you)
 *
 */
static void jump_to_container(xcb_connection_t *conn, const char *arguments) {
        int ws, row, col;
        int result;

        result = sscanf(arguments, "%d %d %d", &ws, &col, &row);
        LOG("Jump called with %d parameters (\"%s\")\n", result, arguments);

        /* No match? Either no arguments were specified, or no numbers */
        if (result < 1) {
                ELOG("At least one valid argument required\n");
                return;
        }

        /* Move to the target workspace */
        workspace_show(conn, ws);

        if (result < 3)
                return;

        DLOG("Boundary-checking col %d, row %d... (max cols %d, max rows %d)\n", col, row, c_ws->cols, c_ws->rows);

        /* Move to row/col */
        if (row >= c_ws->rows)
                row = c_ws->rows - 1;
        if (col >= c_ws->cols)
                col = c_ws->cols - 1;

        DLOG("Jumping to col %d, row %d\n", col, row);
        if (c_ws->table[col][row]->currently_focused != NULL)
                set_focus(conn, c_ws->table[col][row]->currently_focused, true);
}

/*
 * Travels the focus stack by the given number of times (or once, if no argument
 * was specified). That is, selects the window you were in before you focused
 * the current window.
 *
 * The special values 'floating' (select the next floating window), 'tiling'
 * (select the next tiling window), 'ft' (if the current window is floating,
 * select the next tiling window and vice-versa) are also valid
 *
 */
static void travel_focus_stack(xcb_connection_t *conn, const char *arguments) {
        /* Start count at -1 to always skip the first element */
        int times, count = -1;
        Client *current;
        bool floating_criteria;

        /* Either it’s one of the special values… */
        if (strcasecmp(arguments, "floating") == 0) {
                floating_criteria = true;
        } else if (strcasecmp(arguments, "tiling") == 0) {
                floating_criteria = false;
        } else if (strcasecmp(arguments, "ft") == 0) {
                Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
                if (last_focused == SLIST_END(&(c_ws->focus_stack))) {
                        ELOG("Cannot select the next floating/tiling client because there is no client at all\n");
                        return;
                }

                floating_criteria = !client_is_floating(last_focused);
        } else {
                /* …or a number was specified */
                if (sscanf(arguments, "%u", &times) != 1) {
                        ELOG("No or invalid argument given (\"%s\"), using default of 1 times\n", arguments);
                        times = 1;
                }

                SLIST_FOREACH(current, &(CUR_CELL->workspace->focus_stack), focus_clients) {
                        if (++count < times) {
                                DLOG("Skipping\n");
                                continue;
                        }

                        DLOG("Focussing\n");
                        set_focus(conn, current, true);
                        break;
                }
                return;
        }

        /* Select the next client matching the criteria parsed above */
        SLIST_FOREACH(current, &(CUR_CELL->workspace->focus_stack), focus_clients)
                if (client_is_floating(current) == floating_criteria) {
                        set_focus(conn, current, true);
                        break;
                }
}

/* 
 * Switch to next or previous existing workspace
 *
 */
static void next_previous_workspace(xcb_connection_t *conn, int direction) {
        Workspace *ws = c_ws;

        if (direction == 'n') {
                while (1) {
                        ws = TAILQ_NEXT(ws, workspaces);

                        if (ws == TAILQ_END(workspaces))
                                ws = TAILQ_FIRST(workspaces);

                        if (ws == c_ws)
                                return;

                        if (ws->output == NULL)
                                continue;

                        workspace_show(conn, ws->num + 1);
                        return;
                }
        } else if (direction == 'p') {
                while (1) {
                        ws = TAILQ_PREV(ws, workspaces_head, workspaces);

                        if (ws == TAILQ_END(workspaces))
                                ws = TAILQ_LAST(workspaces, workspaces_head);

                        if (ws == c_ws)
                                return;

                        if (ws->output == NULL)
                                continue;

                        workspace_show(conn, ws->num + 1);
                        return;
                }
        }
}

static void parse_move_command(xcb_connection_t *conn, Client *last_focused, const char *command) {
        if (!client_is_floating(last_focused)) {
                LOG("You can only move floating clients with the \"move\" command\n");
                return;
        }

        DLOG("Moving a floating client\n");
        if (STARTS_WITH(command, "left")) {
                command += strlen("left");
                last_focused->rect.x -= atoi(command);
        } else if (STARTS_WITH(command, "right")) {
                command += strlen("right");
                last_focused->rect.x += atoi(command);
        } else if (STARTS_WITH(command, "top")) {
                command += strlen("top");
                last_focused->rect.y -= atoi(command);
        } else if (STARTS_WITH(command, "bottom")) {
                command += strlen("bottom");
                last_focused->rect.y += atoi(command);
        } else {
                ELOG("Syntax: move <left|right|top|bottom> <pixels>\n");
                return;
        }
        /* resize_client flushes */
        resize_client(conn, last_focused);
}

static void parse_resize_command(xcb_connection_t *conn, Client *last_focused, const char *command) {
        int first, second;
        resize_orientation_t orientation = O_VERTICAL;
        Container *con = last_focused->container;
        Workspace *ws = last_focused->workspace;

        if (client_is_floating(last_focused)) {
                DLOG("Resizing a floating client\n");
                if (STARTS_WITH(command, "left")) {
                        command += strlen("left");
                        last_focused->rect.width += atoi(command);
                        last_focused->rect.x -= atoi(command);
                } else if (STARTS_WITH(command, "right")) {
                        command += strlen("right");
                        last_focused->rect.width += atoi(command);
                } else if (STARTS_WITH(command, "top")) {
                        command += strlen("top");
                        last_focused->rect.height += atoi(command);
                        last_focused->rect.y -= atoi(command);
                } else if (STARTS_WITH(command, "bottom")) {
                        command += strlen("bottom");
                        last_focused->rect.height += atoi(command);
                } else {
                        ELOG("Syntax: resize <left|right|top|bottom> [+|-]<pixels>\n");
                        return;
                }

                /* resize_client flushes */
                resize_client(conn, last_focused);

                return;
        }

        if (STARTS_WITH(command, "left")) {
                if (con->col == 0)
                        return;
                first = con->col - 1;
                second = con->col;
                command += strlen("left");
        } else if (STARTS_WITH(command, "right")) {
                first = con->col + (con->colspan - 1);
                DLOG("column %d\n", first);

                if (!cell_exists(ws, first, con->row) ||
                    (first == (ws->cols-1)))
                        return;

                second = first + 1;
                command += strlen("right");
        } else if (STARTS_WITH(command, "top")) {
                if (con->row == 0)
                        return;
                first = con->row - 1;
                second = con->row;
                orientation = O_HORIZONTAL;
                command += strlen("top");
        } else if (STARTS_WITH(command, "bottom")) {
                first = con->row + (con->rowspan - 1);
                if (!cell_exists(ws, con->col, first) ||
                    (first == (ws->rows-1)))
                        return;

                second = first + 1;
                orientation = O_HORIZONTAL;
                command += strlen("bottom");
        } else {
                ELOG("Syntax: resize <left|right|top|bottom> [+|-]<pixels>\n");
                return;
        }

        int pixels = atoi(command);
        if (pixels == 0)
                return;

        resize_container(conn, ws, first, second, orientation, pixels);
}

/*
 * Parses a command, see file CMDMODE for more information
 *
 */
void parse_command(xcb_connection_t *conn, const char *command) {
        LOG("--- parsing command \"%s\" ---\n", command);
        /* Get the first client from focus stack because floating clients are not
         * in any container, therefore CUR_CELL is not appropriate. */
        Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
        if (last_focused == SLIST_END(&(c_ws->focus_stack)))
                last_focused = NULL;

        /* Hmm, just to be sure */
        if (command[0] == '\0')
                return;

        /* Is it an <exec>? Then execute the given command. */
        if (STARTS_WITH(command, "exec ")) {
                LOG("starting \"%s\"\n", command + strlen("exec "));
                start_application(command+strlen("exec "));
                return;
        }

        if (STARTS_WITH(command, "mark")) {
                if (last_focused == NULL) {
                        ELOG("There is no window to mark\n");
                        return;
                }
                const char *rest = command + strlen("mark");
                while (*rest == ' ')
                        rest++;
                if (*rest == '\0') {
                        DLOG("interactive mark starting\n");
                        start_application("i3-input -p 'mark ' -l 1 -P 'Mark: '");
                } else {
                        LOG("mark with \"%s\"\n", rest);
                        client_mark(conn, last_focused, rest);
                }
                return;
        }

        if (STARTS_WITH(command, "goto")) {
                const char *rest = command + strlen("goto");
                while (*rest == ' ')
                        rest++;
                if (*rest == '\0') {
                        DLOG("interactive go to mark starting\n");
                        start_application("i3-input -p 'goto ' -l 1 -P 'Goto: '");
                } else {
                        LOG("go to \"%s\"\n", rest);
                        jump_to_mark(conn, rest);
                }
                return;
        }

        if (STARTS_WITH(command, "stack-limit ")) {
                if (last_focused == NULL || client_is_floating(last_focused)) {
                        ELOG("No container focused\n");
                        return;
                }
                const char *rest = command + strlen("stack-limit ");
                if (strncmp(rest, "rows ", strlen("rows ")) == 0) {
                        last_focused->container->stack_limit = STACK_LIMIT_ROWS;
                        rest += strlen("rows ");
                } else if (strncmp(rest, "cols ", strlen("cols ")) == 0) {
                        last_focused->container->stack_limit = STACK_LIMIT_COLS;
                        rest += strlen("cols ");
                } else {
                        ELOG("Syntax: stack-limit <cols|rows> <limit>\n");
                        return;
                }

                last_focused->container->stack_limit_value = atoi(rest);
                if (last_focused->container->stack_limit_value == 0)
                        last_focused->container->stack_limit = STACK_LIMIT_NONE;

                return;
        }

        if (STARTS_WITH(command, "resize ")) {
                if (last_focused == NULL)
                        return;
                const char *rest = command + strlen("resize ");
                parse_resize_command(conn, last_focused, rest);
                return;
        }

        if (STARTS_WITH(command, "move ")) {
                if (last_focused == NULL)
                        return;
                const char *rest = command + strlen("move ");
                parse_move_command(conn, last_focused, rest);
                return;
        }

        if (STARTS_WITH(command, "mode ")) {
                const char *rest = command + strlen("mode ");
                switch_mode(conn, rest);
                return;
        }

        /* Is it an <exit>? */
        if (STARTS_WITH(command, "exit")) {
                LOG("User issued exit-command, exiting without error.\n");
                restore_geometry(global_conn);
                ipc_shutdown();
                exit(EXIT_SUCCESS);
        }

        /* Is it a <reload>? */
        if (STARTS_WITH(command, "reload")) {
                load_configuration(conn, NULL, true);
                render_layout(conn);
                /* Send an IPC event just in case the ws names have changed */
                ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"reload\"}");
                return;
        }

        /* Is it <restart>? Then restart in place. */
        if (STARTS_WITH(command, "restart")) {
                i3_restart();
        }

        if (STARTS_WITH(command, "kill")) {
                if (last_focused == NULL) {
                        ELOG("There is no window to kill\n");
                        return;
                }

                LOG("Killing current window\n");
                client_kill(conn, last_focused);
                return;
        }

        if (STARTS_WITH(command, "bring ")) {
                const char *arguments = command + strlen("bring ");
                if (arguments[0] == '"')
                        bring_window_here(conn, arguments);
                return;
        }

        /* Is it a jump to a specified workspace, row, col? */
        if (STARTS_WITH(command, "jump ")) {
                const char *arguments = command + strlen("jump ");
                if (arguments[0] == '"')
                        jump_to_window(conn, arguments);
                else jump_to_container(conn, arguments);
                return;
        }

        /* Should we travel the focus stack? */
        if (STARTS_WITH(command, "focus")) {
                const char *arguments = command + strlen("focus ");
                travel_focus_stack(conn, arguments);
                return;
        }

        /* Is it 'f' for fullscreen, or 'fg' for fullscreen_global? */
        if (command[0] == 'f') {
                if (last_focused == NULL)
                        return;
                if (command[1] == 'g')
                        client_toggle_fullscreen_global(conn, last_focused);
                else
                        client_toggle_fullscreen(conn, last_focused);
                return;
        }

        /* Is it just 's' for stacking or 'd' for default? */
        if ((command[0] == 's' || command[0] == 'd' || command[0] == 'T') && (command[1] == '\0')) {
                if (last_focused != NULL && client_is_floating(last_focused)) {
                        ELOG("not switching, this is a floating client\n");
                        return;
                }
                LOG("Switching mode for current container\n");
                int new_mode = MODE_DEFAULT;
                if (command[0] == 's' && CUR_CELL->mode != MODE_STACK)
                        new_mode = MODE_STACK;
                if (command[0] == 'T' && CUR_CELL->mode != MODE_TABBED)
                        new_mode = MODE_TABBED;
                switch_layout_mode(conn, CUR_CELL, new_mode);
                return;
        }

        /* Is it 'bn' (border normal), 'bp' (border 1pixel) or 'bb' (border borderless)? */
        /* or even 'bt' (toggle border: 'bp' -> 'bb' -> 'bn' ) */
        if (command[0] == 'b') {
                if (last_focused == NULL) {
                        ELOG("No window focused, cannot change border type\n");
                        return;
                }

                char com = command[1];
                if (command[1] == 't') {
                        if (last_focused->titlebar_position == TITLEBAR_TOP &&
                            !last_focused->borderless)
                            com = 'p';
                        else if (last_focused->titlebar_position == TITLEBAR_OFF &&
                                 !last_focused->borderless)
                            com = 'b';
                        else com = 'n';
                }

                client_change_border(conn, last_focused, com);
                return;
        }

        if (command[0] == 'H') {
                LOG("Hiding all floating windows\n");
                floating_toggle_hide(conn, c_ws);
                return;
        }

        enum { WITH_WINDOW, WITH_CONTAINER, WITH_WORKSPACE, WITH_SCREEN } with = WITH_WINDOW;

        /* Is it a <with>? */
        if (command[0] == 'w') {
                command++;
                /* TODO: implement */
                if (command[0] == 'c') {
                        with = WITH_CONTAINER;
                        command++;
                } else if (command[0] == 'w') {
                        with = WITH_WORKSPACE;
                        command++;
                } else if (command[0] == 's') {
                        with = WITH_SCREEN;
                        command++;
                } else {
                        ELOG("not yet implemented.\n");
                        return;
                }
        }

        /* Is it 't' for toggle tiling/floating? */
        if (command[0] == 't') {
                if (with == WITH_WORKSPACE) {
                        c_ws->auto_float = !c_ws->auto_float;
                        LOG("autofloat is now %d\n", c_ws->auto_float);
                        return;
                }
                if (last_focused == NULL) {
                        ELOG("Cannot toggle tiling/floating: workspace empty\n");
                        return;
                }

                Workspace *ws = last_focused->workspace;

                if (last_focused->fullscreen)
                        client_leave_fullscreen(conn, last_focused);

                toggle_floating_mode(conn, last_focused, false);
                /* delete all empty columns/rows */
                cleanup_table(conn, ws);

                /* Fix colspan/rowspan if it’d overlap */
                fix_colrowspan(conn, ws);

                render_workspace(conn, ws->output, ws);

                /* Re-focus the client because cleanup_table sets the focus to the last
                 * focused client inside a container only. */
                set_focus(conn, last_focused, true);

                return;
        }

	/* Is it 'n' or 'p' for next/previous workspace? (nw) */
        if ((command[0] == 'n' || command[0] == 'p') && command[1] == 'w') {
               next_previous_workspace(conn, command[0]);
               return;
        }

        /* It’s a normal <cmd> */
        char *rest = NULL;
        enum { ACTION_FOCUS, ACTION_MOVE, ACTION_SNAP } action = ACTION_FOCUS;
        direction_t direction;
        int times = strtol(command, &rest, 10);
        if (rest == NULL) {
                ELOG("Invalid command (\"%s\")\n", command);
                return;
        }

        if (*rest == '\0') {
                /* No rest? This was a workspace number, not a times specification */
                workspace_show(conn, times);
                return;
        }

        if (*rest == 'm' || *rest == 's') {
                action = (*rest == 'm' ? ACTION_MOVE : ACTION_SNAP);
                rest++;
        }

        int workspace = strtol(rest, &rest, 10);

        if (rest == NULL) {
                ELOG("Invalid command (\"%s\")\n", command);
                return;
        }

        if (*rest == '\0') {
                if (last_focused != NULL && client_is_floating(last_focused))
                        move_floating_window_to_workspace(conn, last_focused, workspace);
                else move_current_window_to_workspace(conn, workspace);
                return;
        }

        if (last_focused == NULL) {
                ELOG("Not performing (no window found)\n");
                return;
        }

        if (client_is_floating(last_focused) &&
            (action != ACTION_FOCUS && action != ACTION_MOVE)) {
                ELOG("Not performing (floating)\n");
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
                        ELOG("unknown direction: %c\n", *rest);
                        return;
                }
                rest++;

                if (action == ACTION_FOCUS) {
                        if (with == WITH_SCREEN) {
                                focus_thing(conn, direction, THING_SCREEN);
                                continue;
                        }
                        if (client_is_floating(last_focused)) {
                                floating_focus_direction(conn, last_focused, direction);
                                continue;
                        }
                        focus_thing(conn, direction, (with == WITH_WINDOW ? THING_WINDOW : THING_CONTAINER));
                        continue;
                }

                if (action == ACTION_MOVE) {
                        if (with == WITH_SCREEN) {
                                /* TODO: this should swap the screen’s contents
                                 * (e.g. all workspaces) with the next/previous/…
                                 * screen */
                                ELOG("Not yet implemented\n");
                                continue;
                        }
                        if (client_is_floating(last_focused)) {
                                floating_move(conn, last_focused, direction);
                                continue;
                        }
                        if (with == WITH_WINDOW)
                                move_current_window(conn, direction);
                        else move_current_container(conn, direction);
                        continue;
                }

                if (action == ACTION_SNAP) {
                        if (with == WITH_SCREEN) {
                                ELOG("You cannot snap a screen (it makes no sense).\n");
                                continue;
                        }
                        snap_current_container(conn, direction);
                        continue;
                }
        }
}
