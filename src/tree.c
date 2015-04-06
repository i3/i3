#undef I3__FILE__
#define I3__FILE__ "tree.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * tree.c: Everything that primarily modifies the layout tree data structure.
 *
 */
#include "all.h"

struct Con *croot;
struct Con *focused;

struct all_cons_head all_cons = TAILQ_HEAD_INITIALIZER(all_cons);

/*
 * Create the pseudo-output __i3. Output-independent workspaces such as
 * __i3_scratch will live there.
 *
 */
static Con *_create___i3(void) {
    Con *__i3 = con_new(croot, NULL);
    FREE(__i3->name);
    __i3->name = sstrdup("__i3");
    __i3->type = CT_OUTPUT;
    __i3->layout = L_OUTPUT;
    con_fix_percent(croot);
    x_set_name(__i3, "[i3 con] pseudo-output __i3");
    /* For retaining the correct position/size of a scratchpad window, the
     * dimensions of the real outputs should be multiples of the __i3
     * pseudo-output. Ensuring that is the job of scratchpad_fix_resolution()
     * which gets called after this function and after detecting all the
     * outputs (or whenever an output changes). */
    __i3->rect.width = 1280;
    __i3->rect.height = 1024;

    /* Add a content container. */
    DLOG("adding main content container\n");
    Con *content = con_new(NULL, NULL);
    content->type = CT_CON;
    FREE(content->name);
    content->name = sstrdup("content");
    content->layout = L_SPLITH;

    x_set_name(content, "[i3 con] content __i3");
    con_attach(content, __i3, false);

    /* Attach the __i3_scratch workspace. */
    Con *ws = con_new(NULL, NULL);
    ws->type = CT_WORKSPACE;
    ws->num = -1;
    ws->name = sstrdup("__i3_scratch");
    ws->layout = L_SPLITH;
    con_attach(ws, content, false);
    x_set_name(ws, "[i3 con] workspace __i3_scratch");
    ws->fullscreen_mode = CF_OUTPUT;

    return __i3;
}

/*
 * Loads tree from 'path' (used for in-place restarts).
 *
 */
bool tree_restore(const char *path, xcb_get_geometry_reply_t *geometry) {
    char *globbed = resolve_tilde(path);

    if (!path_exists(globbed)) {
        LOG("%s does not exist, not restoring tree\n", globbed);
        free(globbed);
        return false;
    }

    /* TODO: refactor the following */
    croot = con_new(NULL, NULL);
    croot->rect = (Rect){
        geometry->x,
        geometry->y,
        geometry->width,
        geometry->height};
    focused = croot;

    tree_append_json(focused, globbed, NULL);

    DLOG("appended tree, using new root\n");
    croot = TAILQ_FIRST(&(croot->nodes_head));
    DLOG("new root = %p\n", croot);
    Con *out = TAILQ_FIRST(&(croot->nodes_head));
    DLOG("out = %p\n", out);
    Con *ws = TAILQ_FIRST(&(out->nodes_head));
    DLOG("ws = %p\n", ws);

    /* For in-place restarting into v4.2, we need to make sure the new
     * pseudo-output __i3 is present. */
    if (strcmp(out->name, "__i3") != 0) {
        DLOG("Adding pseudo-output __i3 during inplace restart\n");
        Con *__i3 = _create___i3();
        /* Ensure that it is the first output, other places in the code make
         * that assumption. */
        TAILQ_REMOVE(&(croot->nodes_head), __i3, nodes);
        TAILQ_INSERT_HEAD(&(croot->nodes_head), __i3, nodes);
    }

    restore_open_placeholder_windows(croot);

    return true;
}

/*
 * Initializes the tree by creating the root node. The CT_OUTPUT Cons below the
 * root node are created in randr.c for each Output.
 *
 */
void tree_init(xcb_get_geometry_reply_t *geometry) {
    croot = con_new(NULL, NULL);
    FREE(croot->name);
    croot->name = "root";
    croot->type = CT_ROOT;
    croot->layout = L_SPLITH;
    croot->rect = (Rect){
        geometry->x,
        geometry->y,
        geometry->width,
        geometry->height};

    _create___i3();
}

/*
 * Opens an empty container in the current container
 *
 */
Con *tree_open_con(Con *con, i3Window *window) {
    if (con == NULL) {
        /* every focusable Con has a parent (outputs have parent root) */
        con = focused->parent;
        /* If the parent is an output, we are on a workspace. In this case,
         * the new container needs to be opened as a leaf of the workspace. */
        if (con->parent->type == CT_OUTPUT && con->type != CT_DOCKAREA) {
            con = focused;
        }

        /* If the currently focused container is a floating container, we
         * attach the new container to the currently focused spot in its
         * workspace. */
        if (con->type == CT_FLOATING_CON) {
            con = con_descend_tiling_focused(con->parent);
            if (con->type != CT_WORKSPACE)
                con = con->parent;
        }
        DLOG("con = %p\n", con);
    }

    assert(con != NULL);

    /* 3. create the container and attach it to its parent */
    Con *new = con_new(con, window);
    new->layout = L_SPLITH;

    /* 4: re-calculate child->percent for each child */
    con_fix_percent(con);

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
 * Closes the given container including all children.
 * Returns true if the container was killed or false if just WM_DELETE was sent
 * and the window is expected to kill itself.
 *
 * The dont_kill_parent flag is specified when the function calls itself
 * recursively while deleting a containers children.
 *
 * The force_set_focus flag is specified in the case of killing a floating
 * window: tree_close() will be invoked for the CT_FLOATINGCON (the parent
 * container) and focus should be set there.
 *
 */
bool tree_close(Con *con, kill_window_t kill_window, bool dont_kill_parent, bool force_set_focus) {
    bool was_mapped = con->mapped;
    Con *parent = con->parent;

    if (!was_mapped) {
        /* Even if the container itself is not mapped, its children may be
         * mapped (for example split containers don't have a mapped window on
         * their own but usually contain mapped children). */
        was_mapped = _is_con_mapped(con);
    }

    /* remove the urgency hint of the workspace (if set) */
    if (con->urgent) {
        con->urgent = false;
        con_update_parents_urgency(con);
        workspace_update_urgent_flag(con_get_workspace(con));
    }

    /* Get the container which is next focused */
    Con *next = con_next_focused(con);
    DLOG("next = %p, focused = %p\n", next, focused);

    DLOG("closing %p, kill_window = %d\n", con, kill_window);
    Con *child, *nextchild;
    bool abort_kill = false;
    /* We cannot use TAILQ_FOREACH because the children get deleted
     * in their parent’s nodes_head */
    for (child = TAILQ_FIRST(&(con->nodes_head)); child;) {
        nextchild = TAILQ_NEXT(child, nodes);
        DLOG("killing child=%p\n", child);
        if (!tree_close(child, kill_window, true, false))
            abort_kill = true;
        child = nextchild;
    }

    if (abort_kill) {
        DLOG("One of the children could not be killed immediately (WM_DELETE sent), aborting.\n");
        return false;
    }

    if (con->window != NULL) {
        if (kill_window != DONT_KILL_WINDOW) {
            x_window_kill(con->window->id, kill_window);
            return false;
        } else {
            xcb_void_cookie_t cookie;
            /* Ignore any further events by clearing the event mask,
             * unmap the window,
             * then reparent it to the root window. */
            xcb_change_window_attributes(conn, con->window->id,
                                         XCB_CW_EVENT_MASK, (uint32_t[]){XCB_NONE});
            xcb_unmap_window(conn, con->window->id);
            cookie = xcb_reparent_window(conn, con->window->id, root, 0, 0);

            /* Ignore X11 errors for the ReparentWindow request.
             * X11 Errors are returned when the window was already destroyed */
            add_ignore_event(cookie.sequence, 0);

            /* We are no longer handling this window, thus set WM_STATE to
             * WM_STATE_WITHDRAWN (see ICCCM 4.1.3.1) */
            long data[] = {XCB_ICCCM_WM_STATE_WITHDRAWN, XCB_NONE};
            cookie = xcb_change_property(conn, XCB_PROP_MODE_REPLACE,
                                         con->window->id, A_WM_STATE, A_WM_STATE, 32, 2, data);

            /* Remove the window from the save set. All windows in the save set
             * will be mapped when i3 closes its connection (e.g. when
             * restarting). This is not what we want, since some apps keep
             * unmapped windows around and don’t expect them to suddenly be
             * mapped. See http://bugs.i3wm.org/1617 */
            xcb_change_save_set(conn, XCB_SET_MODE_DELETE, con->window->id);

            /* Ignore X11 errors for the ReparentWindow request.
             * X11 Errors are returned when the window was already destroyed */
            add_ignore_event(cookie.sequence, 0);
        }
        ipc_send_window_event("close", con);
        FREE(con->window->class_class);
        FREE(con->window->class_instance);
        i3string_free(con->window->name);
        FREE(con->window->ran_assignments);
        FREE(con->window);
    }

    Con *ws = con_get_workspace(con);

    /* Figure out which container to focus next before detaching 'con'. */
    if (con_is_floating(con)) {
        if (con == focused) {
            DLOG("This is the focused container, i need to find another one to focus. I start looking at ws = %p\n", ws);
            next = con_next_focused(parent);

            dont_kill_parent = true;
            DLOG("Alright, focusing %p\n", next);
        } else {
            next = NULL;
        }
    }

    /* Detach the container so that it will not be rendered anymore. */
    con_detach(con);

    /* disable urgency timer, if needed */
    if (con->urgency_timer != NULL) {
        DLOG("Removing urgency timer of con %p\n", con);
        workspace_update_urgent_flag(ws);
        ev_timer_stop(main_loop, con->urgency_timer);
        FREE(con->urgency_timer);
    }

    if (con->type != CT_FLOATING_CON) {
        /* If the container is *not* floating, we might need to re-distribute
         * percentage values for the resized containers. */
        con_fix_percent(parent);
    }

    /* Render the tree so that the surrounding containers take up the space
     * which 'con' does no longer occupy. If we don’t render here, there will
     * be a gap in our containers and that could trigger an EnterNotify for an
     * underlying container, see ticket #660.
     *
     * Rendering has to be avoided when dont_kill_parent is set (when
     * tree_close calls itself recursively) because the tree is in a
     * non-renderable state during that time. */
    if (!dont_kill_parent)
        tree_render();

    /* kill the X11 part of this container */
    x_con_kill(con);

    if (con_is_floating(con)) {
        DLOG("Container was floating, killing floating container\n");
        tree_close(parent, DONT_KILL_WINDOW, false, (con == focused));
        DLOG("parent container killed\n");
    }

    free(con->name);
    FREE(con->deco_render_params);
    TAILQ_REMOVE(&all_cons, con, all_cons);
    free(con);

    /* in the case of floating windows, we already focused another container
     * when closing the parent, so we can exit now. */
    if (!next) {
        DLOG("No next container, i will just exit now\n");
        return true;
    }

    if (was_mapped || con == focused) {
        if ((kill_window != DONT_KILL_WINDOW) || !dont_kill_parent || con == focused) {
            DLOG("focusing %p / %s\n", next, next->name);
            if (next->type == CT_DOCKAREA) {
                /* Instead of focusing the dockarea, we need to restore focus to the workspace */
                con_focus(con_descend_focused(output_get_content(next->parent)));
            } else {
                if (!force_set_focus && con != focused)
                    DLOG("not changing focus, the container was not focused before\n");
                else
                    con_focus(next);
            }
        } else {
            DLOG("not focusing because we're not killing anybody\n");
        }
    } else {
        DLOG("not focusing, was not mapped\n");
    }

    /* check if the parent container is empty now and close it */
    if (!dont_kill_parent)
        CALL(parent, on_remove_child);

    return true;
}

/*
 * Closes the current container using tree_close().
 *
 */
void tree_close_con(kill_window_t kill_window) {
    assert(focused != NULL);

    /* There *should* be no possibility to focus outputs / root container */
    assert(focused->type != CT_OUTPUT);
    assert(focused->type != CT_ROOT);

    if (focused->type == CT_WORKSPACE) {
        DLOG("Workspaces cannot be close, closing all children instead\n");
        Con *child, *nextchild;
        for (child = TAILQ_FIRST(&(focused->focus_head)); child;) {
            nextchild = TAILQ_NEXT(child, focused);
            DLOG("killing child=%p\n", child);
            tree_close(child, kill_window, false, false);
            child = nextchild;
        }

        return;
    }

    /* Kill con */
    tree_close(focused, kill_window, false, false);
}

/*
 * Splits (horizontally or vertically) the given container by creating a new
 * container which contains the old one and the future ones.
 *
 */
void tree_split(Con *con, orientation_t orientation) {
    if (con_is_floating(con)) {
        DLOG("Floating containers can't be split.\n");
        return;
    }

    if (con->type == CT_WORKSPACE) {
        if (con_num_children(con) < 2) {
            DLOG("Just changing orientation of workspace\n");
            con->layout = (orientation == HORIZ) ? L_SPLITH : L_SPLITV;
            return;
        } else {
            /* if there is more than one container on the workspace
             * move them into a new container and handle this instead */
            con = workspace_encapsulate(con);
        }
    }

    Con *parent = con->parent;

    /* Force re-rendering to make the indicator border visible. */
    con_force_split_parents_redraw(con);

    /* if we are in a container whose parent contains only one
     * child (its split functionality is unused so far), we just change the
     * orientation (more intuitive than splitting again) */
    if (con_num_children(parent) == 1 &&
        (parent->layout == L_SPLITH ||
         parent->layout == L_SPLITV)) {
        parent->layout = (orientation == HORIZ) ? L_SPLITH : L_SPLITV;
        DLOG("Just changing orientation of existing container\n");
        return;
    }

    DLOG("Splitting in orientation %d\n", orientation);

    /* 2: replace it with a new Con */
    Con *new = con_new(NULL, NULL);
    TAILQ_REPLACE(&(parent->nodes_head), con, new, nodes);
    TAILQ_REPLACE(&(parent->focus_head), con, new, focused);
    new->parent = parent;
    new->layout = (orientation == HORIZ) ? L_SPLITH : L_SPLITV;

    /* 3: swap 'percent' (resize factor) */
    new->percent = con->percent;
    con->percent = 0.0;

    /* 4: add it as a child to the new Con */
    con_attach(con, new, false);
}

/*
 * Moves focus one level up. Returns true if focus changed.
 *
 */
bool level_up(void) {
    /* Skip over floating containers and go directly to the grandparent
     * (which should always be a workspace) */
    if (focused->parent->type == CT_FLOATING_CON) {
        con_focus(focused->parent->parent);
        return true;
    }

    /* We can focus up to the workspace, but not any higher in the tree */
    if ((focused->parent->type != CT_CON &&
         focused->parent->type != CT_WORKSPACE) ||
        focused->type == CT_WORKSPACE) {
        ELOG("'focus parent': Focus is already on the workspace, cannot go higher than that.\n");
        return false;
    }
    con_focus(focused->parent);
    return true;
}

/*
 * Moves focus one level down. Returns true if focus changed.
 *
 */
bool level_down(void) {
    /* Go down the focus stack of the current node */
    Con *next = TAILQ_FIRST(&(focused->focus_head));
    if (next == TAILQ_END(&(focused->focus_head))) {
        DLOG("cannot go down\n");
        return false;
    } else if (next->type == CT_FLOATING_CON) {
        /* Floating cons shouldn't be directly focused; try immediately
         * going to the grandchild of the focused con. */
        Con *child = TAILQ_FIRST(&(next->focus_head));
        if (child == TAILQ_END(&(next->focus_head))) {
            DLOG("cannot go down\n");
            return false;
        } else
            next = TAILQ_FIRST(&(next->focus_head));
    }

    con_focus(next);
    return true;
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
void tree_render(void) {
    if (croot == NULL)
        return;

    DLOG("-- BEGIN RENDERING --\n");
    /* Reset map state for all nodes in tree */
    /* TODO: a nicer method to walk all nodes would be good, maybe? */
    mark_unmapped(croot);
    croot->mapped = true;

    render_con(croot, false);

    x_push_changes(croot);
    DLOG("-- END RENDERING --\n");
}

/*
 * Recursive function to walk the tree until a con can be found to focus.
 *
 */
static bool _tree_next(Con *con, char way, orientation_t orientation, bool wrap) {
    /* When dealing with fullscreen containers, it's necessary to go up to the
     * workspace level, because 'focus $dir' will start at the con's real
     * position in the tree, and it may not be possible to get to the edge
     * normally due to fullscreen focusing restrictions. */
    if (con->fullscreen_mode == CF_OUTPUT && con->type != CT_WORKSPACE)
        con = con_get_workspace(con);

    /* Stop recursing at workspaces after attempting to switch to next
     * workspace if possible. */
    if (con->type == CT_WORKSPACE) {
        if (con_get_fullscreen_con(con, CF_GLOBAL)) {
            DLOG("Cannot change workspace while in global fullscreen mode.\n");
            return false;
        }
        Output *current_output = get_output_containing(con->rect.x, con->rect.y);
        Output *next_output;

        if (!current_output)
            return false;
        DLOG("Current output is %s\n", current_output->name);

        /* Try to find next output */
        direction_t direction;
        if (way == 'n' && orientation == HORIZ)
            direction = D_RIGHT;
        else if (way == 'p' && orientation == HORIZ)
            direction = D_LEFT;
        else if (way == 'n' && orientation == VERT)
            direction = D_DOWN;
        else if (way == 'p' && orientation == VERT)
            direction = D_UP;
        else
            return false;

        next_output = get_output_next(direction, current_output, CLOSEST_OUTPUT);
        if (!next_output)
            return false;
        DLOG("Next output is %s\n", next_output->name);

        /* Find visible workspace on next output */
        Con *workspace = NULL;
        GREP_FIRST(workspace, output_get_content(next_output->con), workspace_is_visible(child));

        /* Show next workspace and focus appropriate container if possible. */
        if (!workspace)
            return false;

        workspace_show(workspace);

        /* If a workspace has an active fullscreen container, one of its
         * children should always be focused. The above workspace_show()
         * should be adequate for that, so return. */
        if (con_get_fullscreen_con(workspace, CF_OUTPUT))
            return true;

        Con *focus = con_descend_direction(workspace, direction);

        /* special case: if there was no tiling con to focus and the workspace
         * has a floating con in the focus stack, focus the top of the focus
         * stack (which may be floating) */
        if (focus == workspace)
            focus = con_descend_focused(workspace);

        if (focus) {
            con_focus(focus);
            x_set_warp_to(&(focus->rect));
        }
        return true;
    }

    Con *parent = con->parent;

    if (con->type == CT_FLOATING_CON) {
        if (orientation != HORIZ)
            return false;

        /* left/right focuses the previous/next floating container */
        Con *next;
        if (way == 'n')
            next = TAILQ_NEXT(con, floating_windows);
        else
            next = TAILQ_PREV(con, floating_head, floating_windows);

        /* If there is no next/previous container, wrap */
        if (!next) {
            if (way == 'n')
                next = TAILQ_FIRST(&(parent->floating_head));
            else
                next = TAILQ_LAST(&(parent->floating_head), floating_head);
        }

        /* Still no next/previous container? bail out */
        if (!next)
            return false;

        /* Raise the floating window on top of other windows preserving
         * relative stack order */
        while (TAILQ_LAST(&(parent->floating_head), floating_head) != next) {
            Con *last = TAILQ_LAST(&(parent->floating_head), floating_head);
            TAILQ_REMOVE(&(parent->floating_head), last, floating_windows);
            TAILQ_INSERT_HEAD(&(parent->floating_head), last, floating_windows);
        }

        con_focus(con_descend_focused(next));
        return true;
    }

    /* If the orientation does not match or there is no other con to focus, we
     * need to go higher in the hierarchy */
    if (con_orientation(parent) != orientation ||
        con_num_children(parent) == 1)
        return _tree_next(parent, way, orientation, wrap);

    Con *current = TAILQ_FIRST(&(parent->focus_head));
    /* TODO: when can the following happen (except for floating windows, which
     * are handled above)? */
    if (TAILQ_EMPTY(&(parent->nodes_head))) {
        DLOG("nothing to focus\n");
        return false;
    }

    Con *next;
    if (way == 'n')
        next = TAILQ_NEXT(current, nodes);
    else
        next = TAILQ_PREV(current, nodes_head, nodes);

    if (!next) {
        if (!config.force_focus_wrapping) {
            /* If there is no next/previous container, we check if we can focus one
             * when going higher (without wrapping, though). If so, we are done, if
             * not, we wrap */
            if (_tree_next(parent, way, orientation, false))
                return true;

            if (!wrap)
                return false;
        }

        if (way == 'n')
            next = TAILQ_FIRST(&(parent->nodes_head));
        else
            next = TAILQ_LAST(&(parent->nodes_head), nodes_head);
    }

    /* Don't violate fullscreen focus restrictions. */
    if (!con_fullscreen_permits_focusing(next))
        return false;

    /* 3: focus choice comes in here. at the moment we will go down
     * until we find a window */
    /* TODO: check for window, atm we only go down as far as possible */
    con_focus(con_descend_focused(next));
    return true;
}

/*
 * Changes focus in the given way (next/previous) and given orientation
 * (horizontal/vertical).
 *
 */
void tree_next(char way, orientation_t orientation) {
    _tree_next(focused, way, orientation, true);
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
    if (con->type != CT_CON ||
        parent->layout == L_OUTPUT || /* con == "content" */
        con->window != NULL)
        goto recurse;

    /* Ensure it got only one child */
    child = TAILQ_FIRST(&(con->nodes_head));
    if (child == NULL || TAILQ_NEXT(child, nodes) != NULL)
        goto recurse;

    DLOG("child = %p, con = %p, parent = %p\n", child, con, parent);

    /* The child must have a different orientation than the con but the same as
     * the con’s parent to be redundant */
    if (!con_is_split(con) ||
        !con_is_split(child) ||
        (con->layout != L_SPLITH && con->layout != L_SPLITV) ||
        (child->layout != L_SPLITH && child->layout != L_SPLITV) ||
        con_orientation(con) == con_orientation(child) ||
        con_orientation(child) != con_orientation(parent))
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
    tree_close(con, DONT_KILL_WINDOW, true, false);

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
