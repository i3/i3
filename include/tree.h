/*
 * vim:ts=4:sw=4:expandtab
 */

#ifndef _TREE_H
#define _TREE_H

extern Con *croot;
/* TODO: i am not sure yet how much access to the focused container should
 * be permitted to source files */
extern Con *focused;
TAILQ_HEAD(all_cons_head, Con);
extern struct all_cons_head all_cons;

/**
 * Initializes the tree by creating the root node, adding all RandR outputs
 * to the tree (that means randr_init() has to be called before) and
 * assigning a workspace to each RandR output.
 *
 */
void tree_init();

/**
 * Opens an empty container in the current container
 *
 */
Con *tree_open_con(Con *con);

/**
 * Splits (horizontally or vertically) the given container by creating a new
 * container which contains the old one and the future ones.
 *
 */
void tree_split(Con *con, orientation_t orientation);

/**
 * Moves focus one level up.
 *
 */
void level_up();

/**
 * Moves focus one level down.
 *
 */
void level_down();

/**
 * Renders the tree, that is rendering all outputs using render_con() and
 * pushing the changes to X11 using x_push_changes().
 *
 */
void tree_render();

/**
 * Closes the current container using tree_close().
 *
 */
void tree_close_con();

/**
 * Changes focus in the given way (next/previous) and given orientation
 * (horizontal/vertical).
 *
 */
void tree_next(char way, orientation_t orientation);

/**
 * Moves the current container in the given way (next/previous) and given
 * orientation (horizontal/vertical).
 *
 */
void tree_move(char way, orientation_t orientation);

/**
 * Closes the given container including all children
 *
 */
void tree_close(Con *con, bool kill_window, bool dont_kill_parent);

/**
 * Loads tree from ~/.i3/_restart.json (used for in-place restarts).
 *
 */
bool tree_restore(const char *path);

/**
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
void tree_flatten(Con *child);

#endif
