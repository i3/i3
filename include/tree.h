/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * tree.c: Everything that primarily modifies the layout tree data structure.
 *
 */
#pragma once

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
void tree_init(xcb_get_geometry_reply_t *geometry);

/**
 * Opens an empty container in the current container
 *
 */
Con *tree_open_con(Con *con, i3Window *window);

/**
 * Splits (horizontally or vertically) the given container by creating a new
 * container which contains the old one and the future ones.
 *
 */
void tree_split(Con *con, orientation_t orientation);

/**
 * Moves focus one level up. Returns true if focus changed.
 *
 */
bool level_up(void);

/**
 * Moves focus one level down. Returns true if focus changed.
 *
 */
bool level_down(void);

/**
 * Renders the tree, that is rendering all outputs using render_con() and
 * pushing the changes to X11 using x_push_changes().
 *
 */
void tree_render(void);

/**
 * Closes the current container using tree_close().
 *
 */
void tree_close_con(kill_window_t kill_window);

/**
 * Changes focus in the given way (next/previous) and given orientation
 * (horizontal/vertical).
 *
 */
void tree_next(char way, orientation_t orientation);

/**
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
bool tree_close(Con *con, kill_window_t kill_window, bool dont_kill_parent, bool force_set_focus);

/**
 * Loads tree from ~/.i3/_restart.json (used for in-place restarts).
 *
 */
bool tree_restore(const char *path, xcb_get_geometry_reply_t *geometry);

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
