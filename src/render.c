/*
 * vim:ts=4:sw=4:expandtab
 */

#include "all.h"

/* change this to 'true' if you want to have additional borders around every
 * container (for debugging purposes) */
static bool show_debug_borders = false;

/*
 * "Renders" the given container (and its children), meaning that all rects are
 * updated correctly. Note that this function does not call any xcb_*
 * functions, so the changes are completely done in memory only (and
 * side-effect free). As soon as you call x_push_changes(), the changes will be
 * updated in X11.
 *
 */
void render_con(Con *con, bool render_fullscreen) {
    DLOG("currently rendering node %p / %s / layout %d\n",
            con, con->name, con->layout);
    int children = con_num_children(con);
    DLOG("children: %d, orientation = %d\n", children, con->orientation);

    /* Copy container rect, subtract container border */
    /* This is the actually usable space inside this container for clients */
    Rect rect = con->rect;

    /* Display a border if this is a leaf node. For container nodes, we don’t
     * draw borders (except when in debug mode) */
    if (show_debug_borders) {
        rect.x += 2;
        rect.y += 2;
        rect.width -= 2 * 2;
        rect.height -= 2 * 2;
    }

    int x = rect.x;
    int y = rect.y;

    int i = 0;

    con->mapped = true;

    /* if this container contains a window, set the coordinates */
    if (con->window) {
        /* depending on the border style, the rect of the child window
         * needs to be smaller */
        Rect *inset = &(con->window_rect);
        *inset = (Rect){0, 0, con->rect.width, con->rect.height};
        if (!render_fullscreen)
            *inset = rect_add(*inset, con_border_style_rect(con));

        /* Obey x11 border */
        inset->width -= (2 * con->border_width);
        inset->height -= (2 * con->border_width);

        /* Obey the aspect ratio, if any */
        if (con->proportional_height != 0 &&
            con->proportional_width != 0) {
            DLOG("proportional height = %d, width = %d\n", con->proportional_height, con->proportional_width);
            double new_height = inset->height + 1;
            int new_width = inset->width;

            while (new_height > inset->height) {
                new_height = ((double)con->proportional_height / con->proportional_width) * new_width;

                if (new_height > inset->height)
                    new_width--;
            }
            /* Center the window */
            inset->y += ceil(inset->height / 2) - floor(new_height / 2);
            inset->x += ceil(inset->width / 2) - floor(new_width / 2);

            inset->height = new_height;
            inset->width = new_width;
            DLOG("new_height = %f, new_width = %d\n", new_height, new_width);
        }

        if (con->height_increment > 1) {
            int old_height = inset->height;
            inset->height -= (inset->height - con->base_height) % con->height_increment;
            DLOG("Lost %d pixel due to client's height_increment (%d px, base_height = %d)\n",
                old_height - inset->height, con->height_increment, con->base_height);
        }

        if (con->width_increment > 1) {
            int old_width = inset->width;
            inset->width -= (inset->width - con->base_width) % con->width_increment;
            DLOG("Lost %d pixel due to client's width_increment (%d px, base_width = %d)\n",
                old_width - inset->width, con->width_increment, con->base_width);
        }

        DLOG("child will be at %dx%d with size %dx%d\n", inset->x, inset->y, inset->width, inset->height);
    }

    /* Check for fullscreen nodes */
    Con *fullscreen = con_get_fullscreen_con(con);
    if (fullscreen) {
        DLOG("got fs node: %p\n", fullscreen);
        fullscreen->rect = rect;
        x_raise_con(fullscreen);
        render_con(fullscreen, true);
        return;
    }

    /* find the height for the decorations */
    i3Font *font = load_font(conn, config.font);
    int deco_height = font->height + 5;

    /* precalculate the sizes to be able to correct rounding errors */
    int sizes[children];
    if (con->layout == L_DEFAULT && children > 0) {
        Con *child;
        int i = 0, assigned = 0;
        int total = con->orientation == HORIZ ? rect.width : rect.height;
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            double percentage = child->percent > 0.0 ? child->percent : 1.0 / children;
            assigned += sizes[i++] = percentage * total;
        }
        assert(assigned == total ||
                (assigned > total && assigned - total <= children * 2) ||
                (assigned < total && total - assigned <= children * 2));
        int signal = assigned < total ? 1 : -1;
        while (assigned != total) {
            for (i = 0; i < children && assigned != total; ++i) {
                sizes[i] += signal;
                assigned += signal;
            }
        }
    }

    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {

        /* default layout */
        if (con->layout == L_DEFAULT) {
            if (con->orientation == HORIZ) {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = sizes[i];
                child->rect.height = rect.height;
                x += child->rect.width;
            } else {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = rect.width;
                child->rect.height = sizes[i];
                y += child->rect.height;
            }

            /* first we have the decoration, if this is a leaf node */
            if (con_is_leaf(child) && child->border_style == BS_NORMAL) {
                DLOG("that child is a leaf node, subtracting deco\n");
                /* TODO: make a function for relative coords? */
                child->deco_rect.x = child->rect.x - con->rect.x;
                child->deco_rect.y = child->rect.y - con->rect.y;

                child->rect.y += deco_height;
                child->rect.height -= deco_height;

                child->deco_rect.width = child->rect.width;
                child->deco_rect.height = deco_height;
            }
        }

        /* stacked layout */
        else if (con->layout == L_STACKED) {
            DLOG("stacked con\n");
            child->rect.x = x;
            child->rect.y = y;
            child->rect.width = rect.width;
            child->rect.height = rect.height;

            child->deco_rect.x = x - con->rect.x;
            child->deco_rect.y = y - con->rect.y + (i * deco_height);
            child->deco_rect.width = child->rect.width;
            child->deco_rect.height = deco_height;

            if (children > 1 || (child->border_style != BS_1PIXEL && child->border_style != BS_NONE)) {
                child->rect.y += (deco_height * children);
                child->rect.height -= (deco_height * children);
            }
        }

        /* tabbed layout */
        else if (con->layout == L_TABBED) {
            DLOG("tabbed con\n");
            child->rect.x = x;
            child->rect.y = y;
            child->rect.width = rect.width;
            child->rect.height = rect.height;

            child->deco_rect.width = child->rect.width / children;
            child->deco_rect.height = deco_height;
            child->deco_rect.x = x - con->rect.x + i * child->deco_rect.width;
            child->deco_rect.y = y - con->rect.y;

            if (children > 1 || (child->border_style != BS_1PIXEL && child->border_style != BS_NONE)) {
                child->rect.y += deco_height;
                child->rect.height -= deco_height;
            }
        }

        DLOG("child at (%d, %d) with (%d x %d)\n",
                child->rect.x, child->rect.y, child->rect.width, child->rect.height);
        DLOG("x now %d, y now %d\n", x, y);
        x_raise_con(child);
        render_con(child, false);
        i++;
    }

    /* in a stacking or tabbed container, we ensure the focused client is raised */
    if (con->layout == L_STACKED || con->layout == L_TABBED) {
        Con *foc = TAILQ_FIRST(&(con->focus_head));
        if (foc != TAILQ_END(&(con->focus_head))) {
            DLOG("con %p is stacking, raising %p\n", con, foc);
            x_raise_con(foc);
            /* by rendering the stacked container again, we handle the case
             * that we have a non-leaf-container inside the stack. */
            render_con(foc, false);
        }
    }

    TAILQ_FOREACH(child, &(con->floating_head), floating_windows) {
        DLOG("render floating:\n");
        DLOG("floating child at (%d,%d) with %d x %d\n", child->rect.x, child->rect.y, child->rect.width, child->rect.height);
        x_raise_con(child);
        render_con(child, false);
    }

    DLOG("-- level up\n");
}
