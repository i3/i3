/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * con.c contains all functions which deal with containers directly (creating
 * containers, searching containers, getting specific properties from
 * containers, …).
 *
 */
#include "all.h"

char *colors[] = {
    "#ff0000",
    "#00FF00",
    "#0000FF",
    "#ff00ff",
    "#00ffff",
    "#ffff00",
    "#aa0000",
    "#00aa00",
    "#0000aa",
    "#aa00aa"
};

static void con_on_remove_child(Con *con);

/*
 * Create a new container (and attach it to the given parent, if not NULL).
 * This function initializes the data structures and creates the appropriate
 * X11 IDs using x_con_init().
 *
 */
Con *con_new(Con *parent, i3Window *window) {
    Con *new = scalloc(sizeof(Con));
    new->on_remove_child = con_on_remove_child;
    TAILQ_INSERT_TAIL(&all_cons, new, all_cons);
    new->type = CT_CON;
    new->window = window;
    new->border_style = config.default_border;
    static int cnt = 0;
    DLOG("opening window %d\n", cnt);

    /* TODO: remove window coloring after test-phase */
    DLOG("color %s\n", colors[cnt]);
    new->name = strdup(colors[cnt]);
    //uint32_t cp = get_colorpixel(colors[cnt]);
    cnt++;
    if ((cnt % (sizeof(colors) / sizeof(char*))) == 0)
        cnt = 0;

    x_con_init(new);

    TAILQ_INIT(&(new->floating_head));
    TAILQ_INIT(&(new->nodes_head));
    TAILQ_INIT(&(new->focus_head));
    TAILQ_INIT(&(new->swallow_head));

    if (parent != NULL)
        con_attach(new, parent, false);

    return new;
}

/*
 * Attaches the given container to the given parent. This happens when moving
 * a container or when inserting a new container at a specific place in the
 * tree.
 *
 * ignore_focus is to just insert the Con at the end (useful when creating a
 * new split container *around* some containers, that is, detaching and
 * attaching them in order without wanting to mess with the focus in between).
 *
 */
void con_attach(Con *con, Con *parent, bool ignore_focus) {
    con->parent = parent;
    Con *loop;
    Con *current = NULL;
    struct nodes_head *nodes_head = &(parent->nodes_head);
    struct focus_head *focus_head = &(parent->focus_head);

    /* Workspaces are handled differently: they need to be inserted at the
     * right position. */
    if (con->type == CT_WORKSPACE) {
        DLOG("it's a workspace. num = %d\n", con->num);
        if (con->num == -1 || TAILQ_EMPTY(nodes_head)) {
            TAILQ_INSERT_TAIL(nodes_head, con, nodes);
        } else {
            current = TAILQ_FIRST(nodes_head);
            if (con->num < current->num) {
                /* we need to insert the container at the beginning */
                TAILQ_INSERT_HEAD(nodes_head, con, nodes);
            } else {
                while (current->num != -1 && con->num > current->num) {
                    current = TAILQ_NEXT(current, nodes);
                    if (current == TAILQ_END(nodes_head)) {
                        current = NULL;
                        break;
                    }
                }
                /* we need to insert con after current, if current is not NULL */
                if (current)
                    TAILQ_INSERT_BEFORE(current, con, nodes);
                else TAILQ_INSERT_TAIL(nodes_head, con, nodes);
            }
        }
        goto add_to_focus_head;
    }

    if (con->type == CT_FLOATING_CON) {
        DLOG("Inserting into floating containers\n");
        TAILQ_INSERT_TAIL(&(parent->floating_head), con, floating_windows);
    } else {
        if (!ignore_focus) {
            /* Get the first tiling container in focus stack */
            TAILQ_FOREACH(loop, &(parent->focus_head), focused) {
                if (loop->type == CT_FLOATING_CON)
                    continue;
                current = loop;
                break;
            }
        }

        /* When the container is not a split container (but contains a window)
         * and is attached to a workspace, we check if the user configured a
         * workspace_layout. This is done in workspace_attach_to, which will
         * provide us with the container to which we should attach (either the
         * workspace or a new split container with the configured
         * workspace_layout).
         */
        if (con->window != NULL && parent->type == CT_WORKSPACE) {
            DLOG("Parent is a workspace. Applying default layout...\n");
            Con *target = workspace_attach_to(parent);

            /* Attach the original con to this new split con instead */
            nodes_head = &(target->nodes_head);
            focus_head = &(target->focus_head);
            con->parent = target;
            current = NULL;

            DLOG("done\n");
        }

        /* Insert the container after the tiling container, if found.
         * When adding to a CT_OUTPUT, just append one after another. */
        if (current && parent->type != CT_OUTPUT) {
            DLOG("Inserting con = %p after last focused tiling con %p\n",
                 con, current);
            TAILQ_INSERT_AFTER(nodes_head, current, con, nodes);
        } else TAILQ_INSERT_TAIL(nodes_head, con, nodes);
    }

add_to_focus_head:
    /* We insert to the TAIL because con_focus() will correct this.
     * This way, we have the option to insert Cons without having
     * to focus them. */
    TAILQ_INSERT_TAIL(focus_head, con, focused);
}

/*
 * Detaches the given container from its current parent
 *
 */
void con_detach(Con *con) {
    if (con->type == CT_FLOATING_CON) {
        TAILQ_REMOVE(&(con->parent->floating_head), con, floating_windows);
        TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    } else {
        TAILQ_REMOVE(&(con->parent->nodes_head), con, nodes);
        TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    }
}

/*
 * Sets input focus to the given container. Will be updated in X11 in the next
 * run of x_push_changes().
 *
 */
void con_focus(Con *con) {
    assert(con != NULL);
    DLOG("con_focus = %p\n", con);

    /* 1: set focused-pointer to the new con */
    /* 2: exchange the position of the container in focus stack of the parent all the way up */
    TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    TAILQ_INSERT_HEAD(&(con->parent->focus_head), con, focused);
    if (con->parent->parent != NULL)
        con_focus(con->parent);

    focused = con;
    if (con->urgent) {
        con->urgent = false;
        workspace_update_urgent_flag(con_get_workspace(con));
    }
    DLOG("con_focus done = %p\n", con);
}

/*
 * Returns true when this node is a leaf node (has no children)
 *
 */
bool con_is_leaf(Con *con) {
    return TAILQ_EMPTY(&(con->nodes_head));
}

/*
 * Returns true if this node accepts a window (if the node swallows windows,
 * it might already have swallowed enough and cannot hold any more).
 *
 */
bool con_accepts_window(Con *con) {
    /* 1: workspaces never accept direct windows */
    if (con->type == CT_WORKSPACE)
        return false;

    if (con->orientation != NO_ORIENTATION) {
        DLOG("container %p does not accepts windows, orientation != NO_ORIENTATION\n", con);
        return false;
    }

    /* TODO: if this is a swallowing container, we need to check its max_clients */
    return (con->window == NULL);
}

/*
 * Gets the output container (first container with CT_OUTPUT in hierarchy) this
 * node is on.
 *
 */
Con *con_get_output(Con *con) {
    Con *result = con;
    while (result != NULL && result->type != CT_OUTPUT)
        result = result->parent;
    /* We must be able to get an output because focus can never be set higher
     * in the tree (root node cannot be focused). */
    assert(result != NULL);
    return result;
}

/*
 * Gets the workspace container this node is on.
 *
 */
Con *con_get_workspace(Con *con) {
    Con *result = con;
    while (result != NULL && result->type != CT_WORKSPACE)
        result = result->parent;
    return result;
}

/*
 * Searches parenst of the given 'con' until it reaches one with the specified
 * 'orientation'. Aborts when it comes across a floating_con.
 *
 */
Con *con_parent_with_orientation(Con *con, orientation_t orientation) {
    DLOG("Searching for parent of Con %p with orientation %d\n", con, orientation);
    Con *parent = con->parent;
    if (parent->type == CT_FLOATING_CON)
        return NULL;
    while (con_orientation(parent) != orientation) {
        DLOG("Need to go one level further up\n");
        parent = parent->parent;
        /* Abort when we reach a floating con */
        if (parent && parent->type == CT_FLOATING_CON)
            parent = NULL;
        if (parent == NULL)
            break;
    }
    DLOG("Result: %p\n", parent);
    return parent;
}

/*
 * helper data structure for the breadth-first-search in
 * con_get_fullscreen_con()
 *
 */
struct bfs_entry {
    Con *con;

    TAILQ_ENTRY(bfs_entry) entries;
};

/*
 * Returns the first fullscreen node below this node.
 *
 */
Con *con_get_fullscreen_con(Con *con, int fullscreen_mode) {
    Con *current, *child;

    /* TODO: is breadth-first-search really appropriate? (check as soon as
     * fullscreen levels and fullscreen for containers is implemented) */
    TAILQ_HEAD(bfs_head, bfs_entry) bfs_head = TAILQ_HEAD_INITIALIZER(bfs_head);
    struct bfs_entry *entry = smalloc(sizeof(struct bfs_entry));
    entry->con = con;
    TAILQ_INSERT_TAIL(&bfs_head, entry, entries);

    while (!TAILQ_EMPTY(&bfs_head)) {
        entry = TAILQ_FIRST(&bfs_head);
        current = entry->con;
        if (current != con && current->fullscreen_mode == fullscreen_mode) {
            /* empty the queue */
            while (!TAILQ_EMPTY(&bfs_head)) {
                entry = TAILQ_FIRST(&bfs_head);
                TAILQ_REMOVE(&bfs_head, entry, entries);
                free(entry);
            }
            return current;
        }

        TAILQ_REMOVE(&bfs_head, entry, entries);
        free(entry);

        TAILQ_FOREACH(child, &(current->nodes_head), nodes) {
            entry = smalloc(sizeof(struct bfs_entry));
            entry->con = child;
            TAILQ_INSERT_TAIL(&bfs_head, entry, entries);
        }

        TAILQ_FOREACH(child, &(current->floating_head), floating_windows) {
            entry = smalloc(sizeof(struct bfs_entry));
            entry->con = child;
            TAILQ_INSERT_TAIL(&bfs_head, entry, entries);
        }
    }

    return NULL;
}

/*
 * Returns true if the node is floating.
 *
 */
bool con_is_floating(Con *con) {
    assert(con != NULL);
    DLOG("checking if con %p is floating\n", con);
    return (con->floating >= FLOATING_AUTO_ON);
}

/*
 * Checks if the given container is either floating or inside some floating
 * container. It returns the FLOATING_CON container.
 *
 */
Con *con_inside_floating(Con *con) {
    assert(con != NULL);
    if (con->type == CT_FLOATING_CON)
        return con;

    if (con->floating >= FLOATING_AUTO_ON)
        return con->parent;

    if (con->type == CT_WORKSPACE || con->type == CT_OUTPUT)
        return NULL;

    return con_inside_floating(con->parent);
}

/*
 * Returns the container with the given client window ID or NULL if no such
 * container exists.
 *
 */
Con *con_by_window_id(xcb_window_t window) {
    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons)
        if (con->window != NULL && con->window->id == window)
            return con;
    return NULL;
}

/*
 * Returns the container with the given frame ID or NULL if no such container
 * exists.
 *
 */
Con *con_by_frame_id(xcb_window_t frame) {
    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons)
        if (con->frame == frame)
            return con;
    return NULL;
}

/*
 * Returns the first container below 'con' which wants to swallow this window
 * TODO: priority
 *
 */
Con *con_for_window(Con *con, i3Window *window, Match **store_match) {
    Con *child;
    Match *match;
    DLOG("searching con for window %p starting at con %p\n", window, con);
    DLOG("class == %s\n", window->class_class);

    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        TAILQ_FOREACH(match, &(child->swallow_head), matches) {
            if (!match_matches_window(match, window))
                continue;
            if (store_match != NULL)
                *store_match = match;
            return child;
        }
        Con *result = con_for_window(child, window, store_match);
        if (result != NULL)
            return result;
    }

    TAILQ_FOREACH(child, &(con->floating_head), floating_windows) {
        TAILQ_FOREACH(match, &(child->swallow_head), matches) {
            if (!match_matches_window(match, window))
                continue;
            if (store_match != NULL)
                *store_match = match;
            return child;
        }
        Con *result = con_for_window(child, window, store_match);
        if (result != NULL)
            return result;
    }

    return NULL;
}

/*
 * Returns the number of children of this container.
 *
 */
int con_num_children(Con *con) {
    Con *child;
    int children = 0;

    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        children++;

    return children;
}

/*
 * Updates the percent attribute of the children of the given container. This
 * function needs to be called when a window is added or removed from a
 * container.
 *
 */
void con_fix_percent(Con *con) {
    Con *child;
    int children = con_num_children(con);

    // calculate how much we have distributed and how many containers
    // with a percentage set we have
    double total = 0.0;
    int children_with_percent = 0;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        if (child->percent > 0.0) {
            total += child->percent;
            ++children_with_percent;
        }
    }

    // if there were children without a percentage set, set to a value that
    // will make those children proportional to all others
    if (children_with_percent != children) {
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            if (child->percent <= 0.0) {
                if (children_with_percent == 0)
                    total += (child->percent = 1.0);
                else total += (child->percent = total / children_with_percent);
            }
        }
    }

    // if we got a zero, just distribute the space equally, otherwise
    // distribute according to the proportions we got
    if (total == 0.0) {
        TAILQ_FOREACH(child, &(con->nodes_head), nodes)
            child->percent = 1.0 / children;
    } else if (total != 1.0) {
        TAILQ_FOREACH(child, &(con->nodes_head), nodes)
            child->percent /= total;
    }
}

/*
 * Toggles fullscreen mode for the given container. Fullscreen mode will not be
 * entered when there already is a fullscreen container on this workspace.
 *
 */
void con_toggle_fullscreen(Con *con, int fullscreen_mode) {
    Con *workspace, *fullscreen;

    if (con->type == CT_WORKSPACE) {
        DLOG("You cannot make a workspace fullscreen.\n");
        return;
    }

    DLOG("toggling fullscreen for %p / %s\n", con, con->name);
    if (con->fullscreen_mode == CF_NONE) {
        /* 1: check if there already is a fullscreen con */
        if (fullscreen_mode == CF_GLOBAL)
            fullscreen = con_get_fullscreen_con(croot, CF_GLOBAL);
        else {
            workspace = con_get_workspace(con);
            fullscreen = con_get_fullscreen_con(workspace, CF_OUTPUT);
        }
        if (fullscreen != NULL) {
            LOG("Not entering fullscreen mode, container (%p/%s) "
                "already is in fullscreen mode\n",
                fullscreen, fullscreen->name);
            goto update_netwm_state;
        }

        /* 2: enable fullscreen */
        con->fullscreen_mode = fullscreen_mode;
    } else {
        /* 1: disable fullscreen */
        con->fullscreen_mode = CF_NONE;
    }

update_netwm_state:
    DLOG("mode now: %d\n", con->fullscreen_mode);

    /* update _NET_WM_STATE if this container has a window */
    /* TODO: when a window is assigned to a container which is already
     * fullscreened, this state needs to be pushed to the client, too */
    if (con->window == NULL)
        return;

    uint32_t values[1];
    unsigned int num = 0;

    if (con->fullscreen_mode != CF_NONE)
        values[num++] = A__NET_WM_STATE_FULLSCREEN;

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->window->id,
                        A__NET_WM_STATE, XCB_ATOM_ATOM, 32, num, values);
}

/*
 * Moves the given container to the currently focused container on the given
 * workspace.
 *
 * The dont_warp flag disables pointer warping and will be set when this
 * function is called while dragging a floating window.
 *
 * TODO: is there a better place for this function?
 *
 */
void con_move_to_workspace(Con *con, Con *workspace, bool dont_warp) {
    if (con->type == CT_WORKSPACE) {
        DLOG("Moving workspaces is not yet implemented.\n");
        return;
    }

    if (con_is_floating(con)) {
        DLOG("Using FLOATINGCON instead\n");
        con = con->parent;
    }

    Con *source_output = con_get_output(con),
        *dest_output = con_get_output(workspace);

    /* 1: save the container which is going to be focused after the current
     * container is moved away */
    Con *focus_next = con_next_focused(con);

    /* 2: get the focused container of this workspace */
    Con *next = con_descend_focused(workspace);

    /* 3: we go up one level, but only when next is a normal container */
    if (next->type != CT_WORKSPACE) {
        DLOG("next originally = %p / %s / type %d\n", next, next->name, next->type);
        next = next->parent;
    }

    /* 4: if the target container is floating, we get the workspace instead.
     * Only tiling windows need to get inserted next to the current container.
     * */
    Con *floatingcon = con_inside_floating(next);
    if (floatingcon != NULL) {
        DLOG("floatingcon, going up even further\n");
        next = floatingcon->parent;
    }

    if (con->type == CT_FLOATING_CON) {
        Con *ws = con_get_workspace(next);
        DLOG("This is a floating window, using workspace %p / %s\n", ws, ws->name);
        next = ws;
    }

    /* If moving to a visible workspace, call show so it can be considered
     * focused. Must do before attaching because workspace_show checks to see
     * if focused container is in its area. */
    if (source_output != dest_output &&
        workspace_is_visible(workspace)) {
        workspace_show(workspace->name);

        if (con->type == CT_FLOATING_CON) {
            DLOG("Floating window, fixing coordinates\n");
            /* Take the relative coordinates of the current output, then add them
             * to the coordinate space of the correct output */
            uint32_t rel_x = (con->rect.x - source_output->rect.x);
            uint32_t rel_y = (con->rect.y - source_output->rect.y);
            con->rect.x = dest_output->rect.x + rel_x;
            con->rect.y = dest_output->rect.y + rel_y;
        }

        /* Don’t warp if told so (when dragging floating windows with the
         * mouse for example) */
        if (dont_warp)
            x_set_warp_to(NULL);
        else
            x_set_warp_to(&(con->rect));
    }

    DLOG("Re-attaching container to %p / %s\n", next, next->name);
    /* 5: re-attach the con to the parent of this focused container */
    Con *parent = con->parent;
    con_detach(con);
    con_attach(con, next, false);

    /* 6: fix the percentages */
    con_fix_percent(parent);
    con->percent = 0.0;
    con_fix_percent(next);

    /* 7: focus the con on the target workspace (the X focus is only updated by
     * calling tree_render(), so for the "real" focus this is a no-op). */
    con_focus(con_descend_focused(con));

    /* 8: when moving to a visible workspace on a different output, we keep the
     * con focused. Otherwise, we leave the focus on the current workspace as we
     * don’t want to focus invisible workspaces */
    if (source_output != dest_output &&
        workspace_is_visible(workspace)) {
        DLOG("Moved to a different output, focusing target\n");
    } else {
        /* Descend focus stack in case focus_next is a workspace which can
         * occur if we move to the same workspace.  Also show current workspace
         * to ensure it is focused. */
        workspace_show(con_get_workspace(focus_next)->name);
        con_focus(con_descend_focused(focus_next));
    }

    CALL(parent, on_remove_child);
}

/*
 * Returns the orientation of the given container (for stacked containers,
 * vertical orientation is used regardless of the actual orientation of the
 * container).
 *
 */
int con_orientation(Con *con) {
    /* stacking containers behave like they are in vertical orientation */
    if (con->layout == L_STACKED)
        return VERT;

    if (con->layout == L_TABBED)
        return HORIZ;

    return con->orientation;
}

/*
 * Returns the container which will be focused next when the given container
 * is not available anymore. Called in tree_close and con_move_to_workspace
 * to properly restore focus.
 *
 */
Con *con_next_focused(Con *con) {
    Con *next;
    /* floating containers are attached to a workspace, so we focus either the
     * next floating container (if any) or the workspace itself. */
    if (con->type == CT_FLOATING_CON) {
        DLOG("selecting next for CT_FLOATING_CON\n");
        next = TAILQ_NEXT(con, floating_windows);
        DLOG("next = %p\n", next);
        if (!next) {
            next = TAILQ_PREV(con, floating_head, floating_windows);
            DLOG("using prev, next = %p\n", next);
        }
        if (!next) {
            Con *ws = con_get_workspace(con);
            next = ws;
            DLOG("no more floating containers for next = %p, restoring workspace focus\n", next);
            while (next != TAILQ_END(&(ws->focus_head)) && !TAILQ_EMPTY(&(next->focus_head))) {
                next = TAILQ_FIRST(&(next->focus_head));
                if (next == con) {
                    DLOG("skipping container itself, we want the next client\n");
                    next = TAILQ_NEXT(next, focused);
                }
            }
            if (next == TAILQ_END(&(ws->focus_head))) {
                DLOG("Focus list empty, returning ws\n");
                next = ws;
            }
        } else {
            /* Instead of returning the next CT_FLOATING_CON, we descend it to
             * get an actual window to focus. */
            next = con_descend_focused(next);
        }
        return next;
    }

    /* dock clients cannot be focused, so we focus the workspace instead */
    if (con->parent->type == CT_DOCKAREA) {
        DLOG("selecting workspace for dock client\n");
        return con_descend_focused(output_get_content(con->parent->parent));
    }

    /* if 'con' is not the first entry in the focus stack, use the first one as
     * it’s currently focused already */
    Con *first = TAILQ_FIRST(&(con->parent->focus_head));
    if (first != con) {
        DLOG("Using first entry %p\n", first);
        next = first;
    } else {
        /* try to focus the next container on the same level as this one or fall
         * back to its parent */
        if (!(next = TAILQ_NEXT(con, focused)))
            next = con->parent;
    }

    /* now go down the focus stack as far as
     * possible, excluding the current container */
    while (!TAILQ_EMPTY(&(next->focus_head)) &&
           TAILQ_FIRST(&(next->focus_head)) != con)
        next = TAILQ_FIRST(&(next->focus_head));

    return next;
}

/*
 * Get the next/previous container in the specified orientation. This may
 * travel up until it finds a container with suitable orientation.
 *
 */
Con *con_get_next(Con *con, char way, orientation_t orientation) {
    DLOG("con_get_next(way=%c, orientation=%d)\n", way, orientation);
    /* 1: get the first parent with the same orientation */
    Con *cur = con;
    while (con_orientation(cur->parent) != orientation) {
        DLOG("need to go one level further up\n");
        if (cur->parent->type == CT_WORKSPACE) {
            LOG("that's a workspace, we can't go further up\n");
            return NULL;
        }
        cur = cur->parent;
    }

    /* 2: chose next (or previous) */
    Con *next;
    if (way == 'n') {
        next = TAILQ_NEXT(cur, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(parent->nodes_head)))
            return NULL;
    } else {
        next = TAILQ_PREV(cur, nodes_head, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(cur->nodes_head)))
            return NULL;
    }
    DLOG("next = %p\n", next);

    return next;
}

/*
 * Returns the focused con inside this client, descending the tree as far as
 * possible. This comes in handy when attaching a con to a workspace at the
 * currently focused position, for example.
 *
 */
Con *con_descend_focused(Con *con) {
    Con *next = con;
    while (!TAILQ_EMPTY(&(next->focus_head)))
        next = TAILQ_FIRST(&(next->focus_head));
    return next;
}

/*
 * Returns the focused con inside this client, descending the tree as far as
 * possible. This comes in handy when attaching a con to a workspace at the
 * currently focused position, for example.
 *
 * Works like con_descend_focused but considers only tiling cons.
 *
 */
Con *con_descend_tiling_focused(Con *con) {
    Con *next = con;
    Con *before;
    Con *child;
    do {
        before = next;
        TAILQ_FOREACH(child, &(next->focus_head), focused) {
            if (child->type == CT_FLOATING_CON)
                continue;

            next = child;
            break;
        }
    } while (before != next);
    return next;
}

/*
 * Returns the leftmost, rightmost, etc. container in sub-tree. For example, if
 * direction is D_LEFT, then we return the rightmost container and if direction
 * is D_RIGHT, we return the leftmost container.  This is because if we are
 * moving D_LEFT, and thus want the rightmost container.
 *
 */
Con *con_descend_direction(Con *con, direction_t direction) {
    Con *most = NULL;
    DLOG("con_descend_direction(%p, %d)\n", con, direction);
    if (direction == D_LEFT || direction == D_RIGHT) {
        if (con->orientation == HORIZ) {
            /* If the direction is horizontal, we can use either the first
             * (D_RIGHT) or the last con (D_LEFT) */
            if (direction == D_RIGHT)
                most = TAILQ_FIRST(&(con->nodes_head));
            else most = TAILQ_LAST(&(con->nodes_head), nodes_head);
        } else if (con->orientation == VERT) {
            /* Wrong orientation. We use the last focused con. Within that con,
             * we recurse to chose the left/right con or at least the last
             * focused one. */
            most = TAILQ_FIRST(&(con->focus_head));
        } else {
            /* If the con has no orientation set, it’s not a split container
             * but a container with a client window, so stop recursing */
            return con;
        }
    }

    if (direction == D_UP || direction == D_DOWN) {
        if (con->orientation == VERT) {
            /* If the direction is vertical, we can use either the first
             * (D_DOWN) or the last con (D_UP) */
            if (direction == D_UP)
                most = TAILQ_LAST(&(con->nodes_head), nodes_head);
            else most = TAILQ_FIRST(&(con->nodes_head));
        } else if (con->orientation == HORIZ) {
            /* Wrong orientation. We use the last focused con. Within that con,
             * we recurse to chose the top/bottom con or at least the last
             * focused one. */
            most = TAILQ_FIRST(&(con->focus_head));
        } else {
            /* If the con has no orientation set, it’s not a split container
             * but a container with a client window, so stop recursing */
            return con;
        }
    }

    if (!most)
        return con;
    return con_descend_direction(most, direction);
}

/*
 * Returns a "relative" Rect which contains the amount of pixels that need to
 * be added to the original Rect to get the final position (obviously the
 * amount of pixels for normal, 1pixel and borderless are different).
 *
 */
Rect con_border_style_rect(Con *con) {
    switch (con_border_style(con)) {
    case BS_NORMAL:
        return (Rect){2, 0, -(2 * 2), -2};

    case BS_1PIXEL:
        return (Rect){1, 1, -2, -2};

    case BS_NONE:
        return (Rect){0, 0, 0, 0};

    default:
        assert(false);
    }
}

/*
 * Use this function to get a container’s border style. This is important
 * because when inside a stack, the border style is always BS_NORMAL.
 * For tabbed mode, the same applies, with one exception: when the container is
 * borderless and the only element in the tabbed container, the border is not
 * rendered.
 *
 * For children of a CT_DOCKAREA, the border style is always none.
 *
 */
int con_border_style(Con *con) {
    Con *fs = con_get_fullscreen_con(con->parent, CF_OUTPUT);
    if (fs == con) {
        DLOG("this one is fullscreen! overriding BS_NONE\n");
        return BS_NONE;
    }

    if (con->parent->layout == L_STACKED)
        return (con_num_children(con->parent) == 1 ? con->border_style : BS_NORMAL);

    if (con->parent->layout == L_TABBED && con->border_style != BS_NORMAL)
        return (con_num_children(con->parent) == 1 ? con->border_style : BS_NORMAL);

    if (con->parent->type == CT_DOCKAREA)
        return BS_NONE;

    return con->border_style;
}

/*
 * This function changes the layout of a given container. Use it to handle
 * special cases like changing a whole workspace to stacked/tabbed (creates a
 * new split container before).
 *
 */
void con_set_layout(Con *con, int layout) {
    /* When the container type is CT_WORKSPACE, the user wants to change the
     * whole workspace into stacked/tabbed mode. To do this and still allow
     * intuitive operations (like level-up and then opening a new window), we
     * need to create a new split container. */
    if (con->type == CT_WORKSPACE) {
        DLOG("Creating new split container\n");
        /* 1: create a new split container */
        Con *new = con_new(NULL, NULL);
        new->parent = con;

        /* 2: set the requested layout on the split con */
        new->layout = layout;

        /* 3: While the layout is irrelevant in stacked/tabbed mode, it needs
         * to be set. Otherwise, this con will not be interpreted as a split
         * container. */
        if (config.default_orientation == NO_ORIENTATION) {
            new->orientation = (con->rect.height > con->rect.width) ? VERT : HORIZ;
        } else {
            new->orientation = config.default_orientation;
        }

        Con *old_focused = TAILQ_FIRST(&(con->focus_head));
        if (old_focused == TAILQ_END(&(con->focus_head)))
            old_focused = NULL;

        /* 4: move the existing cons of this workspace below the new con */
        DLOG("Moving cons\n");
        Con *child;
        while (!TAILQ_EMPTY(&(con->nodes_head))) {
            child = TAILQ_FIRST(&(con->nodes_head));
            con_detach(child);
            con_attach(child, new, true);
        }

        /* 4: attach the new split container to the workspace */
        DLOG("Attaching new split to ws\n");
        con_attach(new, con, false);

        if (old_focused)
            con_focus(old_focused);

        tree_flatten(croot);

        return;
    }

    con->layout = layout;
}

/*
 * Callback which will be called when removing a child from the given con.
 * Kills the container if it is empty and replaces it with the child if there
 * is exactly one child.
 *
 */
static void con_on_remove_child(Con *con) {
    DLOG("on_remove_child\n");

    /* Every container 'above' (in the hierarchy) the workspace content should
     * not be closed when the last child was removed */
    if (con->type == CT_WORKSPACE ||
        con->type == CT_OUTPUT ||
        con->type == CT_ROOT ||
        con->type == CT_DOCKAREA) {
        DLOG("not handling, type = %d\n", con->type);
        return;
    }

    /* TODO: check if this container would swallow any other client and
     * don’t close it automatically. */
    int children = con_num_children(con);
    if (children == 0) {
        DLOG("Container empty, closing\n");
        tree_close(con, DONT_KILL_WINDOW, false);
        return;
    }
}

/*
 * Determines the minimum size of the given con by looking at its children (for
 * split/stacked/tabbed cons). Will be called when resizing floating cons
 *
 */
Rect con_minimum_size(Con *con) {
    DLOG("Determining minimum size for con %p\n", con);

    if (con_is_leaf(con)) {
        DLOG("leaf node, returning 75x50\n");
        return (Rect){ 0, 0, 75, 50 };
    }

    if (con->type == CT_FLOATING_CON) {
        DLOG("floating con\n");
        Con *child = TAILQ_FIRST(&(con->nodes_head));
        return con_minimum_size(child);
    }

    if (con->layout == L_STACKED || con->layout == L_TABBED) {
        uint32_t max_width = 0, max_height = 0, deco_height = 0;
        Con *child;
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            Rect min = con_minimum_size(child);
            deco_height += child->deco_rect.height;
            max_width = max(max_width, min.width);
            max_height = max(max_height, min.height);
        }
        DLOG("stacked/tabbed now, returning %d x %d + deco_rect = %d\n",
             max_width, max_height, deco_height);
        return (Rect){ 0, 0, max_width, max_height + deco_height };
    }

    /* For horizontal/vertical split containers we sum up the width (h-split)
     * or height (v-split) and use the maximum of the height (h-split) or width
     * (v-split) as minimum size. */
    if (con->orientation == HORIZ || con->orientation == VERT) {
        uint32_t width = 0, height = 0;
        Con *child;
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            Rect min = con_minimum_size(child);
            if (con->orientation == HORIZ) {
                width += min.width;
                height = max(height, min.height);
            } else {
                height += min.height;
                width = max(width, min.width);
            }
        }
        DLOG("split container, returning width = %d x height = %d\n", width, height);
        return (Rect){ 0, 0, width, height };
    }

    ELOG("Unhandled case, type = %d, layout = %d, orientation = %d\n",
         con->type, con->layout, con->orientation);
    assert(false);
}
