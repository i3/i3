/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * render.c: Renders (determines position/sizes) the layout tree, updating the
 *           various rects. Needs to be pushed to X11 (see x.c) to be visible.
 *
 */
#pragma once

#include <config.h>

/**
 * This is used to keep a state to pass around when rendering a con in render_con().
 *
 */
typedef struct render_params {
    /* A copy of the coordinates of the container which is being rendered. */
    int x;
    int y;

    /* The computed height for decorations. */
    int deco_height;
    /* Container rect, subtract container border. This is the actually usable space
     * inside this container for clients. */
    Rect rect;
    /* The number of children of the container which is being rendered. */
    int children;
    /* A precalculated list of sizes of each child. */
    int *sizes;
} render_params;

/**
 * "Renders" the given container (and its children), meaning that all rects are
 * updated correctly. Note that this function does not call any xcb_*
 * functions, so the changes are completely done in memory only (and
 * side-effect free). As soon as you call x_push_changes(), the changes will be
 * updated in X11.
 *
 */
void render_con(Con *con);

/**
 * Returns the height for the decorations
 *
 */
int render_deco_height(void);
