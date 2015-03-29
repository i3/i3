#undef I3__FILE__
#define I3__FILE__ "render.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * render.c: Renders (determines position/sizes) the layout tree, updating the
 *           various rects. Needs to be pushed to X11 (see x.c) to be visible.
 *
 */
#include "all.h"

/* change this to 'true' if you want to have additional borders around every
 * container (for debugging purposes) */
static bool show_debug_borders = false;

/*
 * Returns the height for the decorations
 */
int render_deco_height(void) {
    int deco_height = config.font.height + 4;
    if (config.font.height & 0x01)
        ++deco_height;
    return deco_height;
}

/*
 * Renders a container with layout L_OUTPUT. In this layout, all CT_DOCKAREAs
 * get the height of their content and the remaining CT_CON gets the rest.
 *
 */
static void render_l_output(Con *con) {
    Con *child, *dockchild;

    int x = con->rect.x;
    int y = con->rect.y;
    int height = con->rect.height;

    /* Find the content container and ensure that there is exactly one. Also
     * check for any non-CT_DOCKAREA clients. */
    Con *content = NULL;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        if (child->type == CT_CON) {
            if (content != NULL) {
                DLOG("More than one CT_CON on output container\n");
                assert(false);
            }
            content = child;
        } else if (child->type != CT_DOCKAREA) {
            DLOG("Child %p of type %d is inside the OUTPUT con\n", child, child->type);
            assert(false);
        }
    }

    if (content == NULL) {
        DLOG("Skipping this output because it is currently being destroyed.\n");
        return;
    }

    /* We need to find out if there is a fullscreen con on the current workspace
     * and take the short-cut to render it directly (the user does not want to
     * see the dockareas in that case) */
    Con *ws = con_get_fullscreen_con(content, CF_OUTPUT);
    if (!ws) {
        DLOG("Skipping this output because it is currently being destroyed.\n");
        return;
    }
    Con *fullscreen = con_get_fullscreen_con(ws, CF_OUTPUT);
    if (fullscreen) {
        fullscreen->rect = con->rect;
        x_raise_con(fullscreen);
        render_con(fullscreen, true);
        return;
    }

    /* First pass: determine the height of all CT_DOCKAREAs (the sum of their
     * children) and figure out how many pixels we have left for the rest */
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        if (child->type != CT_DOCKAREA)
            continue;

        child->rect.height = 0;
        TAILQ_FOREACH(dockchild, &(child->nodes_head), nodes)
        child->rect.height += dockchild->geometry.height;

        height -= child->rect.height;
    }

    /* Second pass: Set the widths/heights */
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        if (child->type == CT_CON) {
            child->rect.x = x;
            child->rect.y = y;
            child->rect.width = con->rect.width;
            child->rect.height = height;
        }

        child->rect.x = x;
        child->rect.y = y;
        child->rect.width = con->rect.width;

        child->deco_rect.x = 0;
        child->deco_rect.y = 0;
        child->deco_rect.width = 0;
        child->deco_rect.height = 0;

        y += child->rect.height;

        DLOG("child at (%d, %d) with (%d x %d)\n",
             child->rect.x, child->rect.y, child->rect.width, child->rect.height);
        x_raise_con(child);
        render_con(child, false);
    }
}

/*
 * "Renders" the given container (and its children), meaning that all rects are
 * updated correctly. Note that this function does not call any xcb_*
 * functions, so the changes are completely done in memory only (and
 * side-effect free). As soon as you call x_push_changes(), the changes will be
 * updated in X11.
 *
 */
void render_con(Con *con, bool render_fullscreen) {
    int children = con_num_children(con);
    DLOG("Rendering %snode %p / %s / layout %d / children %d\n",
         (render_fullscreen ? "fullscreen " : ""), con, con->name, con->layout,
         children);

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

        /* Obey the aspect ratio, if any, unless we are in fullscreen mode.
         *
         * The spec isn’t explicit on whether the aspect ratio hints should be
         * respected during fullscreen mode. Other WMs such as Openbox don’t do
         * that, and this post suggests that this is the correct way to do it:
         * http://mail.gnome.org/archives/wm-spec-list/2003-May/msg00007.html
         *
         * Ignoring aspect ratio during fullscreen was necessary to fix MPlayer
         * subtitle rendering, see http://bugs.i3wm.org/594 */
        if (!render_fullscreen &&
            con->aspect_ratio > 0.0) {
            DLOG("aspect_ratio = %f, current width/height are %d/%d\n",
                 con->aspect_ratio, inset->width, inset->height);
            double new_height = inset->height + 1;
            int new_width = inset->width;

            while (new_height > inset->height) {
                new_height = (1.0 / con->aspect_ratio) * new_width;

                if (new_height > inset->height)
                    new_width--;
            }
            /* Center the window */
            inset->y += ceil(inset->height / 2) - floor((new_height + .5) / 2);
            inset->x += ceil(inset->width / 2) - floor(new_width / 2);

            inset->height = new_height + .5;
            inset->width = new_width;
        }

        /* NB: We used to respect resize increment size hints for tiling
         * windows up until commit 0db93d9 here. However, since all terminal
         * emulators cope with ignoring the size hints in a better way than we
         * can (by providing their fake-transparency or background color), this
         * code was removed. See also http://bugs.i3wm.org/540 */

        DLOG("child will be at %dx%d with size %dx%d\n", inset->x, inset->y, inset->width, inset->height);
    }

    /* Check for fullscreen nodes */
    Con *fullscreen = NULL;
    if (con->type != CT_OUTPUT) {
        fullscreen = con_get_fullscreen_con(con, (con->type == CT_ROOT ? CF_GLOBAL : CF_OUTPUT));
    }
    if (fullscreen) {
        fullscreen->rect = rect;
        x_raise_con(fullscreen);
        render_con(fullscreen, true);
        /* Fullscreen containers are either global (underneath the CT_ROOT
         * container) or per-output (underneath the CT_CONTENT container). For
         * global fullscreen containers, we cannot abort rendering here yet,
         * because the floating windows (with popup_during_fullscreen smart)
         * have not yet been rendered (see the CT_ROOT code path below). See
         * also http://bugs.i3wm.org/1393 */
        if (con->type != CT_ROOT) {
            return;
        }
    }

    /* find the height for the decorations */
    int deco_height = render_deco_height();

    /* precalculate the sizes to be able to correct rounding errors */
    int sizes[children];
    memset(sizes, 0, children * sizeof(int));
    if ((con->layout == L_SPLITH || con->layout == L_SPLITV) && children > 0) {
        assert(!TAILQ_EMPTY(&con->nodes_head));
        Con *child;
        int i = 0, assigned = 0;
        int total = con_orientation(con) == HORIZ ? rect.width : rect.height;
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

    if (con->layout == L_OUTPUT) {
        /* Skip i3-internal outputs */
        if (con_is_internal(con))
            return;
        render_l_output(con);
    } else if (con->type == CT_ROOT) {
        Con *output;
        if (!fullscreen) {
            TAILQ_FOREACH(output, &(con->nodes_head), nodes) {
                render_con(output, false);
            }
        }

        /* We need to render floating windows after rendering all outputs’
         * tiling windows because they need to be on top of *every* output at
         * all times. This is important when the user places floating
         * windows/containers so that they overlap on another output. */
        DLOG("Rendering floating windows:\n");
        TAILQ_FOREACH(output, &(con->nodes_head), nodes) {
            if (con_is_internal(output))
                continue;
            /* Get the active workspace of that output */
            Con *content = output_get_content(output);
            if (!content || TAILQ_EMPTY(&(content->focus_head))) {
                DLOG("Skipping this output because it is currently being destroyed.\n");
                continue;
            }
            Con *workspace = TAILQ_FIRST(&(content->focus_head));
            Con *fullscreen = con_get_fullscreen_con(workspace, CF_OUTPUT);
            Con *child;
            TAILQ_FOREACH(child, &(workspace->floating_head), floating_windows) {
                /* Don’t render floating windows when there is a fullscreen window
                 * on that workspace. Necessary to make floating fullscreen work
                 * correctly (ticket #564). */
                /* If there is no fullscreen->window, this cannot be a
                 * transient window, so we _know_ we need to skip it. This
                 * happens during restarts where the container already exists,
                 * but the window was not yet associated. */
                if (fullscreen != NULL && fullscreen->window == NULL)
                    continue;
                if (fullscreen != NULL && fullscreen->window != NULL) {
                    Con *floating_child = con_descend_focused(child);
                    Con *transient_con = floating_child;
                    bool is_transient_for = false;
                    /* Exception to the above rule: smart
                     * popup_during_fullscreen handling (popups belonging to
                     * the fullscreen app will be rendered). */
                    while (transient_con != NULL &&
                           transient_con->window != NULL &&
                           transient_con->window->transient_for != XCB_NONE) {
                        DLOG("transient_con = 0x%08x, transient_con->window->transient_for = 0x%08x, fullscreen_id = 0x%08x\n",
                             transient_con->window->id, transient_con->window->transient_for, fullscreen->window->id);
                        if (transient_con->window->transient_for == fullscreen->window->id) {
                            is_transient_for = true;
                            break;
                        }
                        Con *next_transient = con_by_window_id(transient_con->window->transient_for);
                        if (next_transient == NULL)
                            break;
                        /* Some clients (e.g. x11-ssh-askpass) actually set
                         * WM_TRANSIENT_FOR to their own window id, so break instead of
                         * looping endlessly. */
                        if (transient_con == next_transient)
                            break;
                        transient_con = next_transient;
                    }

                    if (!is_transient_for)
                        continue;
                    else {
                        DLOG("Rendering floating child even though in fullscreen mode: "
                             "floating->transient_for (0x%08x) --> fullscreen->id (0x%08x)\n",
                             floating_child->window->transient_for, fullscreen->window->id);
                    }
                }
                DLOG("floating child at (%d,%d) with %d x %d\n",
                     child->rect.x, child->rect.y, child->rect.width, child->rect.height);
                x_raise_con(child);
                render_con(child, false);
            }
        }

    } else {
        /* FIXME: refactor this into separate functions: */
        Con *child;
        TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
            assert(children > 0);

            /* default layout */
            if (con->layout == L_SPLITH || con->layout == L_SPLITV) {
                if (con->layout == L_SPLITH) {
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
                if (con_is_leaf(child)) {
                    if (child->border_style == BS_NORMAL) {
                        /* TODO: make a function for relative coords? */
                        child->deco_rect.x = child->rect.x - con->rect.x;
                        child->deco_rect.y = child->rect.y - con->rect.y;

                        child->rect.y += deco_height;
                        child->rect.height -= deco_height;

                        child->deco_rect.width = child->rect.width;
                        child->deco_rect.height = deco_height;
                    } else {
                        child->deco_rect.x = 0;
                        child->deco_rect.y = 0;
                        child->deco_rect.width = 0;
                        child->deco_rect.height = 0;
                    }
                }
            }

            /* stacked layout */
            else if (con->layout == L_STACKED) {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = rect.width;
                child->rect.height = rect.height;

                child->deco_rect.x = x - con->rect.x;
                child->deco_rect.y = y - con->rect.y + (i * deco_height);
                child->deco_rect.width = child->rect.width;
                child->deco_rect.height = deco_height;

                if (children > 1 || (child->border_style != BS_PIXEL && child->border_style != BS_NONE)) {
                    child->rect.y += (deco_height * children);
                    child->rect.height -= (deco_height * children);
                }
            }

            /* tabbed layout */
            else if (con->layout == L_TABBED) {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = rect.width;
                child->rect.height = rect.height;

                child->deco_rect.width = floor((float)child->rect.width / children);
                child->deco_rect.x = x - con->rect.x + i * child->deco_rect.width;
                child->deco_rect.y = y - con->rect.y;

                /* Since the tab width may be something like 31,6 px per tab, we
             * let the last tab have all the extra space (0,6 * children). */
                if (i == (children - 1)) {
                    child->deco_rect.width += (child->rect.width - (child->deco_rect.x + child->deco_rect.width));
                }

                if (children > 1 || (child->border_style != BS_PIXEL && child->border_style != BS_NONE)) {
                    child->rect.y += deco_height;
                    child->rect.height -= deco_height;
                    child->deco_rect.height = deco_height;
                } else {
                    child->deco_rect.height = (child->border_style == BS_PIXEL ? 1 : 0);
                }
            }

            /* dockarea layout */
            else if (con->layout == L_DOCKAREA) {
                child->rect.x = x;
                child->rect.y = y;
                child->rect.width = rect.width;
                child->rect.height = child->geometry.height;

                child->deco_rect.x = 0;
                child->deco_rect.y = 0;
                child->deco_rect.width = 0;
                child->deco_rect.height = 0;
                y += child->rect.height;
            }

            DLOG("child at (%d, %d) with (%d x %d)\n",
                 child->rect.x, child->rect.y, child->rect.width, child->rect.height);
            x_raise_con(child);
            render_con(child, false);
            i++;
        }

        /* in a stacking or tabbed container, we ensure the focused client is raised */
        if (con->layout == L_STACKED || con->layout == L_TABBED) {
            TAILQ_FOREACH_REVERSE(child, &(con->focus_head), focus_head, focused)
            x_raise_con(child);
            if ((child = TAILQ_FIRST(&(con->focus_head)))) {
                /* By rendering the stacked container again, we handle the case
             * that we have a non-leaf-container inside the stack. In that
             * case, the children of the non-leaf-container need to be raised
             * aswell. */
                render_con(child, false);
            }

            if (children != 1)
                /* Raise the stack con itself. This will put the stack decoration on
             * top of every stack window. That way, when a new window is opened in
             * the stack, the old window will not obscure part of the decoration
             * (it’s unmapped afterwards). */
                x_raise_con(con);
        }
    }
}
