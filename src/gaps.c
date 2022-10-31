/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * gaps.c: gaps logic: whether to display gaps at all, and how big
 *         they should be.
 *
 */
#include "all.h"

/**
 * Calculates the effective gap sizes for a container.
 */
gaps_t calculate_effective_gaps(Con *con) {
    Con *workspace = con_get_workspace(con);
    if (workspace == NULL)
        return (gaps_t){0, 0, 0, 0, 0};

    bool one_child = con_num_visible_children(workspace) <= 1 ||
                     (con_num_children(workspace) == 1 &&
                      (TAILQ_FIRST(&(workspace->nodes_head))->layout == L_TABBED ||
                       TAILQ_FIRST(&(workspace->nodes_head))->layout == L_STACKED));

    if (config.smart_gaps == SMART_GAPS_ON && one_child)
        return (gaps_t){0, 0, 0, 0, 0};

    gaps_t gaps = {
        .inner = (workspace->gaps.inner + config.gaps.inner) / 2,
        .top = 0,
        .right = 0,
        .bottom = 0,
        .left = 0};

    if (config.smart_gaps != SMART_GAPS_INVERSE_OUTER || one_child) {
        gaps.top = workspace->gaps.top + config.gaps.top;
        gaps.right = workspace->gaps.right + config.gaps.right;
        gaps.bottom = workspace->gaps.bottom + config.gaps.bottom;
        gaps.left = workspace->gaps.left + config.gaps.left;
    }

    /* Outer gaps are added on top of inner gaps. */
    gaps.top += 2 * gaps.inner;
    gaps.right += 2 * gaps.inner;
    gaps.bottom += 2 * gaps.inner;
    gaps.left += 2 * gaps.inner;

    return gaps;
}

/*
 * Decides whether the container should be inset.
 */
bool gaps_should_inset_con(Con *con, int children) {
    /* Inset direct children of the workspace that are leaf containers or
       stacked/tabbed containers. */
    if (con->parent != NULL &&
        con->parent->type == CT_WORKSPACE &&
        (con_is_leaf(con) ||
         (con->layout == L_STACKED || con->layout == L_TABBED))) {
        return true;
    }

    /* Inset direct children of vertical or horizontal split containers at any
       depth in the tree (only leaf containers, not split containers within
       split containers, to avoid double insets). */
    if (con_is_leaf(con) &&
        con->parent != NULL &&
        con->parent->type == CT_CON &&
        (con->parent->layout == L_SPLITH ||
         con->parent->layout == L_SPLITV)) {
        return true;
    }

    return false;
}

/*
 * Returns whether the given container has an adjacent container in the
 * specified direction. In other words, this returns true if and only if
 * the container is not touching the edge of the screen in that direction.
 */
bool gaps_has_adjacent_container(Con *con, direction_t direction) {
    Con *workspace = con_get_workspace(con);
    Con *fullscreen = con_get_fullscreen_con(workspace, CF_GLOBAL);
    if (fullscreen == NULL)
        fullscreen = con_get_fullscreen_con(workspace, CF_OUTPUT);

    /* If this container is fullscreen by itself, there's no adjacent container. */
    if (con == fullscreen)
        return false;

    Con *first = con;
    Con *second = NULL;
    bool found_neighbor = resize_find_tiling_participants(&first, &second, direction, false);
    if (!found_neighbor)
        return false;

    /* If we have an adjacent container and nothing is fullscreen, we consider it. */
    if (fullscreen == NULL)
        return true;

    /* For fullscreen containers, only consider the adjacent container if it is also fullscreen. */
    return con_has_parent(con, fullscreen) && con_has_parent(second, fullscreen);
}
