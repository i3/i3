/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * con.c: Functions which deal with containers directly (creating containers,
 *        searching containers, getting specific properties from containers,
 *        …).
 *
 */
#ifndef _CON_H
#define _CON_H

/**
 * Create a new container (and attach it to the given parent, if not NULL).
 * This function initializes the data structures and creates the appropriate
 * X11 IDs using x_con_init().
 *
 */
Con *con_new(Con *parent, i3Window *window);

/**
 * Sets input focus to the given container. Will be updated in X11 in the next
 * run of x_push_changes().
 *
 */
void con_focus(Con *con);

/**
 * Returns true when this node is a leaf node (has no children)
 *
 */
bool con_is_leaf(Con *con);

/**
 * Returns true if this node accepts a window (if the node swallows windows,
 * it might already have swallowed enough and cannot hold any more).
 *
 */
bool con_accepts_window(Con *con);

/**
 * Gets the output container (first container with CT_OUTPUT in hierarchy) this
 * node is on.
 *
 */
Con *con_get_output(Con *con);

/**
 * Gets the workspace container this node is on.
 *
 */
Con *con_get_workspace(Con *con);

/**
 * Searches parenst of the given 'con' until it reaches one with the specified
 * 'orientation'. Aborts when it comes across a floating_con.
 *
 */
Con *con_parent_with_orientation(Con *con, orientation_t orientation);

/**
 * Returns the first fullscreen node below this node.
 *
 */
Con *con_get_fullscreen_con(Con *con, int fullscreen_mode);

/**
 * Returns true if the node is floating.
 *
 */
bool con_is_floating(Con *con);

/**
 * Checks if the given container is either floating or inside some floating
 * container. It returns the FLOATING_CON container.
 *
 */
Con *con_inside_floating(Con *con);

/**
 * Checks if the given container is inside a focused container.
 *
 */
bool con_inside_focused(Con *con);

/**
 * Returns the container with the given client window ID or NULL if no such
 * container exists.
 *
 */
Con *con_by_window_id(xcb_window_t window);

/**
 * Returns the container with the given frame ID or NULL if no such container
 * exists.
 *
 */
Con *con_by_frame_id(xcb_window_t frame);

/**
 * Returns the first container below 'con' which wants to swallow this window
 * TODO: priority
 *
 */
Con *con_for_window(Con *con, i3Window *window, Match **store_match);

/**
 * Returns the number of children of this container.
 *
 */
int con_num_children(Con *con);

/**
 * Attaches the given container to the given parent. This happens when moving
 * a container or when inserting a new container at a specific place in the
 * tree.
 *
 * ignore_focus is to just insert the Con at the end (useful when creating a
 * new split container *around* some containers, that is, detaching and
 * attaching them in order without wanting to mess with the focus in between).
 *
 */
void con_attach(Con *con, Con *parent, bool ignore_focus);

/**
 * Detaches the given container from its current parent
 *
 */
void con_detach(Con *con);

/**
 * Updates the percent attribute of the children of the given container. This
 * function needs to be called when a window is added or removed from a
 * container.
 *
 */
void con_fix_percent(Con *con);

/**
 * Toggles fullscreen mode for the given container. Fullscreen mode will not be
 * entered when there already is a fullscreen container on this workspace.
 *
 */
void con_toggle_fullscreen(Con *con, int fullscreen_mode);

/**
 * Moves the given container to the currently focused container on the given
 * workspace.
 *
 * The fix_coordinates flag will translate the current coordinates (offset from
 * the monitor position basically) to appropriate coordinates on the
 * destination workspace.
 * Not enabling this behaviour comes in handy when this function gets called by
 * floating_maybe_reassign_ws, which will only "move" a floating window when it
 * *already* changed its coordinates to a different output.
 *
 * The dont_warp flag disables pointer warping and will be set when this
 * function is called while dragging a floating window.
 *
 * TODO: is there a better place for this function?
 *
 */
void con_move_to_workspace(Con *con, Con *workspace, bool fix_coordinates, bool dont_warp);

/**
 * Returns the orientation of the given container (for stacked containers,
 * vertical orientation is used regardless of the actual orientation of the
 * container).
 *
 */
int con_orientation(Con *con);

/**
 * Returns the container which will be focused next when the given container
 * is not available anymore. Called in tree_close and con_move_to_workspace
 * to properly restore focus.
 *
 */
Con *con_next_focused(Con *con);

/**
 * Get the next/previous container in the specified orientation. This may
 * travel up until it finds a container with suitable orientation.
 *
 */
Con *con_get_next(Con *con, char way, orientation_t orientation);

/**
 * Returns the focused con inside this client, descending the tree as far as
 * possible. This comes in handy when attaching a con to a workspace at the
 * currently focused position, for example.
 *
 */
Con *con_descend_focused(Con *con);

/**
 * Returns the focused con inside this client, descending the tree as far as
 * possible. This comes in handy when attaching a con to a workspace at the
 * currently focused position, for example.
 *
 * Works like con_descend_focused but considers only tiling cons.
 *
 */
Con *con_descend_tiling_focused(Con *con);

/*
 * Returns the leftmost, rightmost, etc. container in sub-tree. For example, if
 * direction is D_LEFT, then we return the rightmost container and if direction
 * is D_RIGHT, we return the leftmost container.  This is because if we are
 * moving D_LEFT, and thus want the rightmost container.
 */
Con *con_descend_direction(Con *con, direction_t direction);

/**
 * Returns a "relative" Rect which contains the amount of pixels that need to
 * be added to the original Rect to get the final position (obviously the
 * amount of pixels for normal, 1pixel and borderless are different).
 *
 */
Rect con_border_style_rect(Con *con);

/**
 * Use this function to get a container’s border style. This is important
 * because when inside a stack, the border style is always BS_NORMAL.
 * For tabbed mode, the same applies, with one exception: when the container is
 * borderless and the only element in the tabbed container, the border is not
 * rendered.
 *
 * For children of a CT_DOCKAREA, the border style is always none.
 *
 */
int con_border_style(Con *con);

/**
 * This function changes the layout of a given container. Use it to handle
 * special cases like changing a whole workspace to stacked/tabbed (creates a
 * new split container before).
 *
 */
void con_set_layout(Con *con, int layout);

/**
 * Determines the minimum size of the given con by looking at its children (for
 * split/stacked/tabbed cons). Will be called when resizing floating cons
 *
 */
Rect con_minimum_size(Con *con);

#endif
