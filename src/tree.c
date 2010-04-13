/*
 * vim:ts=4:sw=4:expandtab
 */

#include "all.h"

struct Con *croot;
struct Con *focused;

struct all_cons_head all_cons = TAILQ_HEAD_INITIALIZER(all_cons);

/*
 * Sets input focus to the given container. Will be updated in X11 in the next
 * run of x_push_changes().
 *
 */
void con_focus(Con *con) {
    assert(con != NULL);

    /* 1: set focused-pointer to the new con */
    /* 2: exchange the position of the container in focus stack of the parent all the way up */
    TAILQ_REMOVE(&(con->parent->focus_head), con, focused);
    TAILQ_INSERT_HEAD(&(con->parent->focus_head), con, focused);
    if (con->parent->parent != NULL)
        con_focus(con->parent);

    focused = con;
}

/*
 * Loads tree from ~/.i3/_restart.json
 *
 */
bool tree_restore() {
    char *globbed = glob_path("~/.i3/_restart.json");

    if (!path_exists(globbed)) {
        LOG("%s does not exist, not restoring tree\n", globbed);
        free(globbed);
        return false;
    }

    /* TODO: refactor the following */
    croot = con_new(NULL);
    focused = croot;

    tree_append_json(globbed);
    char *old_restart = glob_path("~/.i3/_restart.json.old");
    unlink(old_restart);
    rename(globbed, old_restart);
    free(globbed);
    free(old_restart);

    printf("appended tree, using new root\n");
    croot = TAILQ_FIRST(&(croot->nodes_head));
    printf("new root = %p\n", croot);
    Con *out = TAILQ_FIRST(&(croot->nodes_head));
    printf("out = %p\n", out);
    Con *ws = TAILQ_FIRST(&(out->nodes_head));
    printf("ws = %p\n", ws);
    con_focus(ws);

    return true;
}

/*
 * Initializes the tree by creating the root node, adding all RandR outputs
 * to the tree (that means randr_init() has to be called before) and
 * assigning a workspace to each RandR output.
 *
 */
void tree_init() {
    Output *output;

    croot = con_new(NULL);
    croot->name = "root";
    croot->type = CT_ROOT;

    Con *ws;
    /* add the outputs */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;

        Con *oc = con_new(croot);
        oc->name = strdup(output->name);
        oc->type = CT_OUTPUT;
        oc->rect = output->rect;

        /* add a workspace to this output */
        ws = con_new(oc);
        ws->name = strdup("1");
        ws->fullscreen_mode = CF_OUTPUT;
    }

    con_focus(ws);
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
    }

    assert(con != NULL);

    /* 3: re-calculate child->percent for each child */
    con_fix_percent(con, WINDOW_ADD);

    /* 4: add a new container leaf to this con */
    Con *new = con_new(con);
    con_focus(new);

    return new;
}

/*
 * Closes the given container including all children
 *
 */
void tree_close(Con *con) {
    /* TODO: check floating clients and adjust old_parent if necessary */

    /* Get the container which is next focused */
    Con *next;
    if (con->type == CT_FLOATING_CON) {
        next = TAILQ_NEXT(con, floating_windows);
        if (next == TAILQ_END(&(con->parent->floating_head)))
            next = con->parent;
    } else {
        next = TAILQ_NEXT(con, focused);
        if (next == TAILQ_END(&(con->parent->nodes_head)))
            next = con->parent;
    }

    LOG("closing %p\n", con);
    Con *child;
    /* We cannot use TAILQ_FOREACH because the children get deleted
     * in their parent’s nodes_head */
    while (!TAILQ_EMPTY(&(con->nodes_head))) {
        child = TAILQ_FIRST(&(con->nodes_head));
        tree_close(child);
    }

    /* kill the X11 part of this container */
    x_con_kill(con);

    con_detach(con);
    con_fix_percent(con->parent, WINDOW_REMOVE);

    if (con->window != NULL) {
        x_window_kill(con->window->id);
        free(con->window);
    }
    free(con->name);
    TAILQ_REMOVE(&all_cons, con, all_cons);
    free(con);

    /* TODO: check if the container (or one of its children) was focused */
    con_focus(next);
}

void tree_close_con() {
    assert(focused != NULL);
    if (focused->parent->type == CT_OUTPUT) {
        LOG("Cannot close workspace\n");
        return;
    }

    /* Kill con */
    tree_close(focused);
}

/*
 * Splits (horizontally or vertically) the given container by creating a new
 * container which contains the old one and the future ones.
 *
 */
void tree_split(Con *con, orientation_t orientation) {
    /* 2: replace it with a new Con */
    Con *new = con_new(NULL);
    Con *parent = con->parent;
    TAILQ_REPLACE(&(parent->nodes_head), con, new, nodes);
    TAILQ_REPLACE(&(parent->focus_head), con, new, focused);
    new->parent = parent;
    new->orientation = orientation;

    /* 3: add it as a child to the new Con */
    con_attach(con, new);
}

void level_up() {
    /* We can focus up to the workspace, but not any higher in the tree */
    if (focused->parent->type != CT_CON) {
        printf("cannot go up\n");
        return;
    }
    con_focus(focused->parent);
}

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
}

void tree_render() {
    if (croot == NULL)
        return;

    printf("-- BEGIN RENDERING --\n");
    /* Reset map state for all nodes in tree */
    /* TODO: a nicer method to walk all nodes would be good, maybe? */
    mark_unmapped(croot);
    croot->mapped = true;

    /* We start rendering at an output */
    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        printf("output %p / %s\n", output, output->name);
        render_con(output);
    }
    x_push_changes(croot);
    printf("-- END RENDERING --\n");
}

void tree_next(char way, orientation_t orientation) {
    /* 1: get the first parent with the same orientation */
    Con *parent = focused->parent;
    while (parent->orientation != orientation) {
        LOG("need to go one level further up\n");
        /* if the current parent is an output, we are at a workspace
         * and the orientation still does not match */
        if (parent->parent->type == CT_OUTPUT)
            return;
        parent = parent->parent;
    }
    Con *current = TAILQ_FIRST(&(parent->focus_head));
    assert(current != TAILQ_END(&(parent->focus_head)));

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
    while (!TAILQ_EMPTY(&(next->focus_head)))
        next = TAILQ_FIRST(&(next->focus_head));

    con_focus(next);
}

void tree_move(char way, orientation_t orientation) {
    /* 1: get the first parent with the same orientation */
    Con *parent = focused->parent;
    bool level_changed = false;
    while (parent->orientation != orientation) {
        LOG("need to go one level further up\n");
        /* if the current parent is an output, we are at a workspace
         * and the orientation still does not match */
        if (parent->parent->type == CT_OUTPUT)
            return;
        parent = parent->parent;
        level_changed = true;
    }
    Con *current = TAILQ_FIRST(&(parent->focus_head));
    assert(current != TAILQ_END(&(parent->focus_head)));

    /* 2: chose next (or previous) */
    Con *next = current;
    if (way == 'n') {
        LOG("i would insert it after %p / %s\n", next, next->name);
        if (!level_changed) {
            next = TAILQ_NEXT(next, nodes);
            if (next == TAILQ_END(&(next->parent->nodes_head))) {
                LOG("cannot move further to the right\n");
                return;
            }
        }

        con_detach(focused);
        focused->parent = next->parent;

        TAILQ_INSERT_AFTER(&(next->parent->nodes_head), next, focused, nodes);
        TAILQ_INSERT_HEAD(&(next->parent->focus_head), focused, focused);
        /* TODO: don’t influence focus handling? */
    } else {
        LOG("i would insert it before %p / %s\n", current, current->name);
        if (!level_changed) {
            next = TAILQ_PREV(next, nodes_head, nodes);
            if (next == TAILQ_END(&(next->parent->nodes_head))) {
                LOG("cannot move further\n");
                return;
            }
        }

        con_detach(focused);
        focused->parent = next->parent;

        TAILQ_INSERT_BEFORE(next, focused, nodes);
        TAILQ_INSERT_HEAD(&(next->parent->focus_head), focused, focused);
        /* TODO: don’t influence focus handling? */
    }
}
