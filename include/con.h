#ifndef _CON_H
#define _CON_H

/**
 * Create a new container (and attach it to the given parent, if not NULL).
 * This function initializes the data structures and creates the appropriate
 * X11 IDs using x_con_init().
 *
 */
Con *con_new(Con *parent);

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
 * Returns the first fullscreen node below this node.
 *
 */
Con *con_get_fullscreen_con(Con *con);

/**
 * Returns true if the node is floating.
 *
 */
bool con_is_floating(Con *con);

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
 * Returns the first container which wants to swallow this window
 * TODO: priority
 *
 */
Con *con_for_window(i3Window *window, Match **store_match);

/**
 * Attaches the given container to the given parent. This happens when moving
 * a container or when inserting a new container at a specific place in the
 * tree.
 *
 */
void con_attach(Con *con, Con *parent);

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
void con_fix_percent(Con *con, int action);
enum { WINDOW_ADD = 0, WINDOW_REMOVE = 1 };

/**
 * Toggles fullscreen mode for the given container. Fullscreen mode will not be
 * entered when there already is a fullscreen container on this workspace.
 *
 */
void con_toggle_fullscreen(Con *con);

/**
 * Moves the given container to the currently focused container on the given
 * workspace.
 * TODO: is there a better place for this function?
 *
 */
void con_move_to_workspace(Con *con, Con *workspace);

/**
 * Returns the orientation of the given container (for stacked containers,
 * vertical orientation is used regardless of the actual orientation of the
 * container).
 *
 */
int con_orientation(Con *con);

#endif
