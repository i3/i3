#line 2 "scratchpad.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * scratchpad.c: Moving windows to the scratchpad and making them visible again.
 *
 */
#include "all.h"

/*
 * Moves the specified window to the __i3_scratch workspace, making it floating
 * and setting the appropriate scratchpad_state.
 *
 * Gets called upon the command 'move scratchpad'.
 *
 */
void scratchpad_move(Con *con) {
    if (con->type == CT_WORKSPACE) {
        LOG("'move scratchpad' used on a workspace \"%s\". Calling it "
            "recursively on all windows on this workspace.\n", con->name);
        Con *current;
        current = TAILQ_FIRST(&(con->focus_head));
        while (current) {
            Con *next = TAILQ_NEXT(current, focused);
            scratchpad_move(current);
            current = next;
        }
        return;
    }
    DLOG("should move con %p to __i3_scratch\n", con);

    Con *__i3_scratch = workspace_get("__i3_scratch", NULL);
    if (con_get_workspace(con) == __i3_scratch) {
        DLOG("This window is already on __i3_scratch.\n");
        return;
    }

    /* 1: Ensure the window is floating. From now on, we deal with the
     * CT_FLOATING_CON. We use automatic == false because the user made the
     * choice that this window should be a scratchpad (and floating). */
    floating_enable(con, false);
    con = con->parent;

    /* 2: Send the window to the __i3_scratch workspace, mainting its
     * coordinates and not warping the pointer. */
    Con *focus_next = con_next_focused(con);
    con_move_to_workspace(con, __i3_scratch, true, true);

    /* 3: If this is the first time this window is used as a scratchpad, we set
     * the scratchpad_state to SCRATCHPAD_FRESH. The window will then be
     * adjusted in size according to what the user specifies. */
    if (con->scratchpad_state == SCRATCHPAD_NONE) {
        DLOG("This window was never used as a scratchpad before.\n");
        con->scratchpad_state = SCRATCHPAD_FRESH;
    }

    /* 4: Fix focus. Normally, when moving a window to a different output, the
     * destination output gets focused. In this case, we don’t want that. */
    con_focus(focus_next);
}

/*
 * Either shows the top-most scratchpad window (con == NULL) or shows the
 * specified con (if it is scratchpad window).
 *
 * When called with con == NULL and the currently focused window is a
 * scratchpad window, this serves as a shortcut to hide it again (so the user
 * can press the same key to quickly look something up).
 *
 */
void scratchpad_show(Con *con) {
    DLOG("should show scratchpad window %p\n", con);
    Con *__i3_scratch = workspace_get("__i3_scratch", NULL);
    Con *floating;

    /* If this was 'scratchpad show' without criteria, we check if the
     * currently focused window is a scratchpad window and should be hidden
     * again. */
    if (!con &&
        (floating = con_inside_floating(focused)) &&
        floating->scratchpad_state != SCRATCHPAD_NONE) {
        DLOG("Focused window is a scratchpad window, hiding it.\n");
        scratchpad_move(focused);
        return;
    }

    /* If this was 'scratchpad show' with criteria, we check if it matches a
     * currently visible scratchpad window and hide it. */
    Con *active = con_get_workspace(focused);
    Con *current = con_get_workspace(con);
    if (con &&
        (floating = con_inside_floating(con)) &&
        floating->scratchpad_state != SCRATCHPAD_NONE &&
        current != __i3_scratch) {
        /* If scratchpad window is on the active workspace, then we should hide
         * it, otherwise we should move it to the active workspace. */
        if (current == active) {
            DLOG("Window is a scratchpad window, hiding it.\n");
            scratchpad_move(con);
            return;
        }
    }

    if (con == NULL) {
        /* Use the container on __i3_scratch which is highest in the focus
         * stack. When moving windows to __i3_scratch, they get inserted at the
         * bottom of the stack. */
        con = TAILQ_FIRST(&(__i3_scratch->floating_head));

        if (!con) {
            LOG("You don't have any scratchpad windows yet.\n");
            LOG("Use 'move scratchpad' to move a window to the scratchpad.\n");
            return;
        }
    }

    /* 1: Move the window from __i3_scratch to the current workspace. */
    con_move_to_workspace(con, active, true, false);

    /* 2: Adjust the size if this window was not adjusted yet. */
    if (con->scratchpad_state == SCRATCHPAD_FRESH) {
        DLOG("Adjusting size of this window.\n");
        Con *output = con_get_output(con);
        con->rect.width = output->rect.width * 0.5;
        con->rect.height = output->rect.height * 0.75;
        con->rect.x = output->rect.x +
                      ((output->rect.width / 2.0) - (con->rect.width / 2.0));
        con->rect.y = output->rect.y +
                      ((output->rect.height / 2.0) - (con->rect.height / 2.0));
        con->scratchpad_state = SCRATCHPAD_CHANGED;
    }

    /* Activate active workspace if window is from another workspace to ensure
     * proper focus. */
    if (current != active) {
        workspace_show(active);
    }

    con_focus(con_descend_focused(con));
}

/*
 * Greatest common divisor, implemented only for the least common multiple
 * below.
 *
 */
static int _gcd(const int m, const int n) {
    if (n == 0)
        return m;
    return _gcd(n, (m % n));
}

/*
 * Least common multiple. We use it to determine the (ideally not too large)
 * resolution for the __i3 pseudo-output on which the scratchpad is on (see
 * below). We could just multiply the resolutions, but for some pathetic cases
 * (many outputs), using the LCM will achieve better results.
 *
 * Man, when you were learning about these two algorithms for the first time,
 * did you think you’d ever need them in a real-world software project of
 * yours? I certainly didn’t until now. :-D
 *
 */
static int _lcm(const int m, const int n) {
    const int o = _gcd(m, n);
    return ((m * n) / o);
}

/*
 * When starting i3 initially (and after each change to the connected outputs),
 * this function fixes the resolution of the __i3 pseudo-output. When that
 * resolution is not set to a function which shares a common divisor with every
 * active output’s resolution, floating point calculation errors will lead to
 * the scratchpad window moving when shown repeatedly.
 *
 */
void scratchpad_fix_resolution(void) {
    Con *__i3_scratch = workspace_get("__i3_scratch", NULL);
    Con *__i3_output = con_get_output(__i3_scratch);
    DLOG("Current resolution: (%d, %d) %d x %d\n",
         __i3_output->rect.x, __i3_output->rect.y,
         __i3_output->rect.width, __i3_output->rect.height);
    Con *output;
    int new_width = -1,
        new_height = -1;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        if (output == __i3_output)
            continue;
        DLOG("output %s's resolution: (%d, %d) %d x %d\n",
             output->name, output->rect.x, output->rect.y,
             output->rect.width, output->rect.height);
        if (new_width == -1) {
            new_width = output->rect.width;
            new_height = output->rect.height;
        } else {
            new_width = _lcm(new_width, output->rect.width);
            new_height = _lcm(new_height, output->rect.height);
        }
    }
    DLOG("new width = %d, new height = %d\n",
         new_width, new_height);
    __i3_output->rect.width = new_width;
    __i3_output->rect.height = new_height;
}
