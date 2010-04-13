/*
 * vim:ts=4:sw=4:expandtab
 */

#include "all.h"

/*
 * "Renders" the given container (and its children), meaning that all rects are
 * updated correctly. Note that this function does not call any xcb_*
 * functions, so the changes are completely done in memory only (and
 * side-effect free). As soon as you call x_push_changes(), the changes will be
 * updated in X11.
 *
 */
void render_con(Con *con) {
    printf("currently rendering node %p / %s / layout %d\n",
            con, con->name, con->layout);
    int children = 0;
    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        children++;
    printf("children: %d, orientation = %d\n", children, con->orientation);

    /* Copy container rect, subtract container border */
    /* This is the actually usable space inside this container for clients */
    Rect rect = con->rect;
    rect.x += 2;
    rect.y += 2;
    rect.width -= 2 * 2;
    rect.height -= 2 * 2;

    int x = rect.x;
    int y = rect.y;

    int i = 0;

    printf("mapped = true\n");
    con->mapped = true;

    /* if this container contains a window, set the coordinates */
    if (con->window) {
        /* depending on the border style, the rect of the child window
         * needs to be smaller */
        Rect *inset = &(con->window_rect);
        *inset = (Rect){0, 0, con->rect.width, con->rect.height};
        /* TODO: different border styles */
        inset->x += 2;
        inset->width -= 2 * 2;
        inset->height -= 2;
    }

    /* Check for fullscreen nodes */
    Con *fullscreen = con_get_fullscreen_con(con);
    if (fullscreen) {
        LOG("got fs node: %p\n", fullscreen);
        fullscreen->rect = rect;
        render_con(fullscreen);
        return;
    }

    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {

        /* default layout */
        if (con->layout == L_DEFAULT) {
            double percentage = 1.0 / children;
            if (child->percent > 0.0)
                percentage = child->percent;
            printf("child %p / %s requests percentage %f\n",
                    child, child->name, percentage);

            if (con->orientation == HORIZ) {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = percentage * rect.width;
                child->rect.height = rect.height;
                x += child->rect.width;
            } else {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = rect.width;
                child->rect.height = percentage * rect.height;
                y += child->rect.height;
            }

            /* first we have the decoration, if this is a leaf node */
            if (con_is_leaf(child)) {
                printf("that child is a leaf node, subtracting deco\n");
                /* TODO: make a function for relative coords? */
                child->deco_rect.x = child->rect.x - con->rect.x;
                child->deco_rect.y = child->rect.y - con->rect.y;

                child->rect.y += 17;
                child->rect.height -= 17;

                child->deco_rect.width = child->rect.width;
                child->deco_rect.height = 17;
            }
        }

        if (con->layout == L_STACKED) {
            printf("stacked con\n");
            child->rect.x = x;
            child->rect.y = y;
            child->rect.width = rect.width;
            child->rect.height = rect.height;

            child->rect.y += (17 * children);
            child->rect.height -= (17 * children);

            child->deco_rect.x = x - con->rect.x;
            child->deco_rect.y = y - con->rect.y + (i * 17);
            child->deco_rect.width = child->rect.width;
            child->deco_rect.height = 17;
        }

        printf("child at (%d, %d) with (%d x %d)\n",
                child->rect.x, child->rect.y, child->rect.width, child->rect.height);
        printf("x now %d, y now %d\n", x, y);
        x_raise_con(child);
        render_con(child);
        i++;
    }

    /* in a stacking container, we ensure the focused client is raised */
    if (con->layout == L_STACKED) {
        Con *foc = TAILQ_FIRST(&(con->focus_head));
        x_raise_con(foc);
    }

    TAILQ_FOREACH(child, &(con->floating_head), floating_windows) {
        LOG("render floating:\n");
        LOG("floating child at (%d,%d) with %d x %d\n", child->rect.x, child->rect.y, child->rect.width, child->rect.height);
        x_raise_con(child);
        render_con(child);
    }

    printf("-- level up\n");
}
