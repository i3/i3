/*
 * vim:ts=4:sw=4:expandtab
 */

#include "all.h"

struct Con *croot;
struct Con *focused;

struct all_cons_head all_cons = TAILQ_HEAD_INITIALIZER(all_cons);

/*
 * Loads tree from ~/.i3/_restart.json (used for in-place restarts).
 *
 */
bool tree_restore(const char *path) {
    char *globbed = resolve_tilde(path);

    if (!path_exists(globbed)) {
        LOG("%s does not exist, not restoring tree\n", globbed);
        free(globbed);
        return false;
    }

    /* TODO: refactor the following */
    croot = con_new(NULL);
    focused = croot;

    tree_append_json(globbed);

    printf("appended tree, using new root\n");
    croot = TAILQ_FIRST(&(croot->nodes_head));
    printf("new root = %p\n", croot);
    Con *out = TAILQ_FIRST(&(croot->nodes_head));
    printf("out = %p\n", out);
    Con *ws = TAILQ_FIRST(&(out->nodes_head));
    printf("ws = %p\n", ws);

    return true;
}

/*
 * Initializes the tree by creating the root node. The CT_OUTPUT Cons below the
 * root node are created in randr.c for each Output.
 *
 */
void tree_init() {
    croot = con_new(NULL);
    FREE(croot->name);
    croot->name = "root";
    croot->type = CT_ROOT;
}

/*
 * Opens an empty container in the current container
 *
 */
Con *tree_open_con(Con *con) {
    if (con == NULL) {
        /* every focusable Con has a parent (outputs have parent root) */
        con = focused->parent;
        /* If the parent is an output, we are on a workspace. In this case,
         * the new container needs to be opened as a leaf of the workspace. */
        if (con->type == CT_OUTPUT)
            con = focused;
        /* If the currently focused container is a floating container, we
         * attach the new container to the workspace */
        if (con->type == CT_FLOATING_CON)
            con = con->parent;
    }

    assert(con != NULL);

    /* 3. create the container and attach it to its parent */
    Con *new = con_new(con);

    /* 4: re-calculate child->percent for each child */
    con_fix_percent(con);

    /* 5: focus the new container */
    con_focus(new);

    return new;
}

static bool _is_con_mapped(Con *con) {
    Con *child;

    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        if (_is_con_mapped(child))
            return true;

    return con->mapped;
}

/*
 * Closes the given container including all children
 *
 */
void tree_close(Con *con, bool kill_window, bool dont_kill_parent) {
    bool was_mapped = con->mapped;
    Con *parent = con->parent;

    if (!was_mapped) {
        /* Even if the container itself is not mapped, its children may be
         * mapped (for example split containers don't have a mapped window on
         * their own but usually contain mapped children). */
        was_mapped = _is_con_mapped(con);
    }

    /* Get the container which is next focused */
    Con *next = con_next_focused(con);
    DLOG("next = %p, focused = %p\n", next, focused);

    DLOG("closing %p, kill_window = %d\n", con, kill_window);
    Con *child;
    /* We cannot use TAILQ_FOREACH because the children get deleted
     * in their parent’s nodes_head */
    while (!TAILQ_EMPTY(&(con->nodes_head))) {
        child = TAILQ_FIRST(&(con->nodes_head));
        DLOG("killing child=%p\n", child);
        tree_close(child, kill_window, true);
    }

    if (con->window != NULL) {
        if (kill_window)
            x_window_kill(con->window->id);
        else {
            /* un-parent the window */
            xcb_reparent_window(conn, con->window->id, root, 0, 0);
            /* TODO: client_unmap to set state to withdrawn */

        }
        FREE(con->window->class_class);
        FREE(con->window->class_instance);
        FREE(con->window->name_x);
        FREE(con->window->name_json);
        free(con->window);
    }

    /* kill the X11 part of this container */
    x_con_kill(con);

    con_detach(con);
    if (con->type != CT_FLOATING_CON) {
        /* If the container is *not* floating, we might need to re-distribute
         * percentage values for the resized containers. */
        con_fix_percent(parent);
    }

    if (con_is_floating(con)) {
        Con *ws = con_get_workspace(con);
        DLOG("Container was floating, killing floating container\n");
        tree_close(parent, false, false);
        DLOG("parent container killed\n");
        if (con == focused) {
            DLOG("This is the focused container, i need to find another one to focus. I start looking at ws = %p\n", ws);
            /* go down the focus stack as far as possible */
            next = con_descend_focused(ws);

            dont_kill_parent = true;
            DLOG("Alright, focusing %p\n", next);
        } else {
            next = NULL;
        }
    }

    free(con->name);
    TAILQ_REMOVE(&all_cons, con, all_cons);
    free(con);

    /* in the case of floating windows, we already focused another container
     * when closing the parent, so we can exit now. */
    if (!next) {
        DLOG("No next container, i will just exit now\n");
        return;
    }

    if (was_mapped || con == focused) {
        if (kill_window || !dont_kill_parent) {
            DLOG("focusing %p / %s\n", next, next->name);
            /* TODO: check if the container (or one of its children) was focused */
            con_focus(next);
        }
        else {
            DLOG("not focusing because we're not killing anybody");
        }
    } else {
        DLOG("not focusing, was not mapped\n");
    }

    /* check if the parent container is empty now and close it */
    if (!dont_kill_parent &&
        parent->type != CT_WORKSPACE &&
        TAILQ_EMPTY(&(parent->nodes_head))) {
        DLOG("Closing empty parent container\n");
        /* TODO: check if this container would swallow any other client and
         * don’t close it automatically. */
        tree_close(parent, false, false);
    }
}

/*
 * Closes the current container using tree_close().
 *
 */
void tree_close_con() {
    assert(focused != NULL);
    if (focused->type == CT_WORKSPACE) {
        LOG("Cannot close workspace\n");
        return;
    }

    /* There *should* be no possibility to focus outputs / root container */
    assert(focused->type != CT_OUTPUT);
    assert(focused->type != CT_ROOT);

    /* Kill con */
    tree_close(focused, true, false);
}

/*
 * Splits (horizontally or vertically) the given container by creating a new
 * container which contains the old one and the future ones.
 *
 */
void tree_split(Con *con, orientation_t orientation) {
    /* for a workspace, we just need to change orientation */
    if (con->type == CT_WORKSPACE) {
        DLOG("Workspace, simply changing orientation to %d\n", orientation);
        con->orientation = orientation;
        return;
    }

    Con *parent = con->parent;
    /* if we are in a container whose parent contains only one
     * child (its split functionality is unused so far), we just change the
     * orientation (more intuitive than splitting again) */
    if (con_num_children(parent) == 1) {
        parent->orientation = orientation;
        DLOG("Just changing orientation of existing container\n");
        return;
    }

    DLOG("Splitting in orientation %d\n", orientation);

    /* 2: replace it with a new Con */
    Con *new = con_new(NULL);
    TAILQ_REPLACE(&(parent->nodes_head), con, new, nodes);
    TAILQ_REPLACE(&(parent->focus_head), con, new, focused);
    new->parent = parent;
    new->orientation = orientation;

    /* 3: swap 'percent' (resize factor) */
    new->percent = con->percent;
    con->percent = 0.0;

    /* 4: add it as a child to the new Con */
    con_attach(con, new, false);
}

/*
 * Moves focus one level up.
 *
 */
void level_up() {
    /* We can focus up to the workspace, but not any higher in the tree */
    if (focused->parent->type != CT_CON &&
        focused->parent->type != CT_WORKSPACE) {
        printf("cannot go up\n");
        return;
    }
    con_focus(focused->parent);
}

/*
 * Moves focus one level down.
 *
 */
void level_down() {
    /* Go down the focus stack of the current node */
    Con *next = TAILQ_FIRST(&(focused->focus_head));
    if (next == TAILQ_END(&(focused->focus_head))) {
        printf("cannot go down\n");
        return;
    }
    con_focus(next);
}

static void mark_unmapped(Con *con) {
    Con *current;

    con->mapped = false;
    TAILQ_FOREACH(current, &(con->nodes_head), nodes)
        mark_unmapped(current);
    if (con->type == CT_WORKSPACE) {
        /* We need to call mark_unmapped on floating nodes aswell since we can
         * make containers floating. */
        TAILQ_FOREACH(current, &(con->floating_head), floating_windows)
            mark_unmapped(current);
    }
}

/*
 * Renders the tree, that is rendering all outputs using render_con() and
 * pushing the changes to X11 using x_push_changes().
 *
 */
void tree_render() {
    if (croot == NULL)
        return;

    DLOG("-- BEGIN RENDERING --\n");
    /* Reset map state for all nodes in tree */
    /* TODO: a nicer method to walk all nodes would be good, maybe? */
    mark_unmapped(croot);
    croot->mapped = true;

    /* We start rendering at an output */
    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        DLOG("output %p / %s\n", output, output->name);
        render_con(output, false);
    }
    x_push_changes(croot);
    DLOG("-- END RENDERING --\n");
}

/*
 * Changes focus in the given way (next/previous) and given orientation
 * (horizontal/vertical).
 *
 */
void tree_next(char way, orientation_t orientation) {
    /* 1: get the first parent with the same orientation */
    Con *parent = focused->parent;
    while (focused->type != CT_WORKSPACE &&
           con_orientation(parent) != orientation) {
        LOG("need to go one level further up\n");
        /* if the current parent is an output, we are at a workspace
         * and the orientation still does not match */
        if (parent->type == CT_WORKSPACE)
            return;
        parent = parent->parent;
    }
    Con *current = TAILQ_FIRST(&(parent->focus_head));
    assert(current != TAILQ_END(&(parent->focus_head)));

    if (TAILQ_EMPTY(&(parent->nodes_head))) {
        DLOG("Nothing to focus here, move along...\n");
        return;
    }

    /* 2: chose next (or previous) */
    Con *next;
    if (way == 'n') {
        next = TAILQ_NEXT(current, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(parent->nodes_head)))
            next = TAILQ_FIRST(&(parent->nodes_head));
    } else {
        next = TAILQ_PREV(current, nodes_head, nodes);
        /* if we are at the end of the list, we need to wrap */
        if (next == TAILQ_END(&(parent->nodes_head)))
            next = TAILQ_LAST(&(parent->nodes_head), nodes_head);
    }

    /* 3: focus choice comes in here. at the moment we will go down
     * until we find a window */
    /* TODO: check for window, atm we only go down as far as possible */
    con_focus(con_descend_focused(next));
}

/*
 * Moves the current container in the given way (next/previous) and given
 * orientation (horizontal/vertical).
 *
 */
void tree_move(char way, orientation_t orientation) {
    /* 1: get the first parent with the same orientation */
    Con *parent = focused->parent;
    Con *old_parent = parent;
    if (focused->type == CT_WORKSPACE)
        return;
    bool level_changed = false;
    while (con_orientation(parent) != orientation) {
        DLOG("need to go one level further up\n");
        /* If the current parent is an output, we are at a workspace
         * and the orientation still does not match. In this case, we split the
         * workspace to have the same look & feel as in older i3 releases. */
        if (parent->type == CT_WORKSPACE) {
            DLOG("Arrived at workspace\n");
            /* In case of moving a window out of a floating con, there might be
             * not a single tiling container. Makes no sense to split then, so
             * just use the workspace as target */
            if (TAILQ_EMPTY(&(parent->nodes_head)))
                break;

            /* 1: create a new split container */
            Con *new = con_new(NULL);
            new->parent = parent;

            /* 2: copy layout and orientation from workspace */
            new->layout = parent->layout;
            new->orientation = parent->orientation;

            Con *old_focused = TAILQ_FIRST(&(parent->focus_head));
            if (old_focused == TAILQ_END(&(parent->focus_head)))
                old_focused = NULL;

            /* 3: move the existing cons of this workspace below the new con */
            DLOG("Moving cons\n");
            Con *child;
            while (!TAILQ_EMPTY(&(parent->nodes_head))) {
                child = TAILQ_FIRST(&(parent->nodes_head));
                con_detach(child);
                con_attach(child, new, true);
            }

            /* 4: switch workspace orientation */
            parent->orientation = orientation;

            /* 5: attach the new split container to the workspace */
            DLOG("Attaching new split to ws\n");
            con_attach(new, parent, false);

            /* 6: fix the percentages */
            con_fix_percent(parent);

            if (old_focused)
                con_focus(old_focused);

            level_changed = true;

            break;
        }
        parent = parent->parent;
        level_changed = true;
    }
    Con *current = TAILQ_FIRST(&(parent->focus_head));
    assert(current != TAILQ_END(&(parent->focus_head)));

    /* If we have no tiling cons (when moving a window out of a floating con to
     * an otherwise empty workspace for example), we just attach the window to
     * the workspace. */
    bool fix_percent = false;
    if (TAILQ_EMPTY(&(parent->nodes_head))) {
        con_detach(focused);
        con_fix_percent(focused->parent);
        focused->parent = parent;

        TAILQ_INSERT_HEAD(&(parent->nodes_head), focused, nodes);
        TAILQ_INSERT_HEAD(&(parent->focus_head), focused, focused);
    } else {
        /* 2: chose next (or previous) */
        Con *next = current;
        if (way == 'n') {
            LOG("i would insert it after %p / %s\n", next, next->name);

            /* Have a look at the next container: If there is no next container or
             * if it is a leaf node, we move the focused one left to it. However,
             * for split containers, we descend into it. */
            next = TAILQ_NEXT(next, nodes);
            if (next == TAILQ_END(&(next->parent->nodes_head))) {
                if (focused == current)
                    return;
                next = current;
            } else {
                if (level_changed && con_is_leaf(next)) {
                    next = current;
                } else {
                    /* if this is a split container, we need to go down */
                    next = con_descend_focused(next);
                }
            }

            con_detach(focused);
            if (focused->parent != next->parent) {
                con_fix_percent(focused->parent);
                focused->parent = next->parent;
                fix_percent = true;
            }

            TAILQ_INSERT_AFTER(&(next->parent->nodes_head), next, focused, nodes);
            TAILQ_INSERT_HEAD(&(next->parent->focus_head), focused, focused);
            /* TODO: don’t influence focus handling? */
        } else {
            LOG("i would insert it before %p / %s\n", current, current->name);
            bool gone_down = false;
            next = TAILQ_PREV(next, nodes_head, nodes);
            if (next == TAILQ_END(&(next->parent->nodes_head))) {
                if (focused == current)
                    return;
                next = current;
            } else {
                if (level_changed && con_is_leaf(next)) {
                    next = current;
                } else {
                    /* if this is a split container, we need to go down */
                    while (!TAILQ_EMPTY(&(next->focus_head))) {
                        gone_down = true;
                        next = TAILQ_FIRST(&(next->focus_head));
                    }
                }
            }

            con_detach(focused);
            if (focused->parent != next->parent) {
                con_fix_percent(focused->parent);
                focused->parent = next->parent;
                fix_percent = true;
            }

            /* After going down in the tree, we insert the container *after*
             * the currently focused one even though the command used "before".
             * This is to keep the user experience clear, since the before/after
             * only signifies the direction of the movement on top-level */
            if (gone_down)
                TAILQ_INSERT_AFTER(&(next->parent->nodes_head), next, focused, nodes);
            else TAILQ_INSERT_BEFORE(next, focused, nodes);
            TAILQ_INSERT_HEAD(&(next->parent->focus_head), focused, focused);
            /* TODO: don’t influence focus handling? */
        }
    }

    /* fix the percentages in the container we moved to */
    if (fix_percent) {
        int children = con_num_children(focused->parent);
        if (children == 1)
            focused->percent = 1.0;
        else
            focused->percent = 1.0 / (children - 1);
        con_fix_percent(focused->parent);
    }

    /* We need to call con_focus() to fix the focus stack "above" the container
     * we just inserted the focused container into (otherwise, the parent
     * container(s) would still point to the old container(s)). */
    con_focus(focused);

    if (con_num_children(old_parent) == 0) {
        DLOG("Old container empty after moving. Let's close it\n");
        tree_close(old_parent, false, false);
    } else if (level_changed) {
        /* fix the percentages in the container we moved from */
        con_fix_percent(old_parent);
    }

    tree_flatten(croot);
}

/*
 * tree_flatten() removes pairs of redundant split containers, e.g.:
 *       [workspace, horizontal]
 *   [v-split]           [child3]
 *   [h-split]
 * [child1] [child2]
 * In this example, the v-split and h-split container are redundant.
 * Such a situation can be created by moving containers in a direction which is
 * not the orientation of their parent container. i3 needs to create a new
 * split container then and if you move containers this way multiple times,
 * redundant chains of split-containers can be the result.
 *
 */
void tree_flatten(Con *con) {
    Con *current, *child, *parent = con->parent;
    DLOG("Checking if I can flatten con = %p / %s\n", con, con->name);

    /* We only consider normal containers without windows */
    if (con->type != CT_CON || con->window != NULL)
        goto recurse;

    /* Ensure it got only one child */
    child = TAILQ_FIRST(&(con->nodes_head));
    if (child == NULL || TAILQ_NEXT(child, nodes) != NULL)
        goto recurse;

    /* The child must have a different orientation than the con but the same as
     * the con’s parent to be redundant */
    if (con->orientation == NO_ORIENTATION ||
        child->orientation == NO_ORIENTATION ||
        con->orientation == child->orientation ||
        child->orientation != parent->orientation)
        goto recurse;

    DLOG("Alright, I have to flatten this situation now. Stay calm.\n");
    /* 1: save focus */
    Con *focus_next = TAILQ_FIRST(&(child->focus_head));

    DLOG("detaching...\n");
    /* 2: re-attach the children to the parent before con */
    while (!TAILQ_EMPTY(&(child->nodes_head))) {
        current = TAILQ_FIRST(&(child->nodes_head));
        DLOG("detaching current=%p / %s\n", current, current->name);
        con_detach(current);
        DLOG("re-attaching\n");
        /* We don’t use con_attach() here because for a CT_CON, the special
         * case handling of con_attach() does not trigger. So all it would do
         * is calling TAILQ_INSERT_AFTER, but with the wrong container. So we
         * directly use the TAILQ macros. */
        current->parent = parent;
        TAILQ_INSERT_BEFORE(con, current, nodes);
        DLOG("attaching to focus list\n");
        TAILQ_INSERT_TAIL(&(parent->focus_head), current, focused);
        current->percent = con->percent;
    }
    DLOG("re-attached all\n");

    /* 3: restore focus, if con was focused */
    if (focus_next != NULL &&
        TAILQ_FIRST(&(parent->focus_head)) == con) {
        DLOG("restoring focus to focus_next=%p\n", focus_next);
        TAILQ_REMOVE(&(parent->focus_head), focus_next, focused);
        TAILQ_INSERT_HEAD(&(parent->focus_head), focus_next, focused);
        DLOG("restored focus.\n");
    }

    /* 4: close the redundant cons */
    DLOG("closing redundant cons\n");
    tree_close(con, false, true);

    /* Well, we got to abort the recursion here because we destroyed the
     * container. However, if tree_flatten() is called sufficiently often,
     * there can’t be the situation of having two pairs of redundant containers
     * at once. Therefore, we can safely abort the recursion on this level
     * after flattening. */
    return;

recurse:
    /* We cannot use normal foreach here because tree_flatten might close the
     * current container. */
    current = TAILQ_FIRST(&(con->nodes_head));
    while (current != NULL) {
        Con *next = TAILQ_NEXT(current, nodes);
        tree_flatten(current);
        current = next;
    }

    current = TAILQ_FIRST(&(con->floating_head));
    while (current != NULL) {
        Con *next = TAILQ_NEXT(current, floating_windows);
        tree_flatten(current);
        current = next;
    }
}
