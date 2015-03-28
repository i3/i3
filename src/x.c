#undef I3__FILE__
#define I3__FILE__ "x.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * x.c: Interface to X11, transfers our in-memory state to X11 (see also
 *      render.c). Basically a big state machine.
 *
 */
#include "all.h"

/* Stores the X11 window ID of the currently focused window */
xcb_window_t focused_id = XCB_NONE;

/* Because 'focused_id' might be reset to force input focus, we separately keep
 * track of the X11 window ID to be able to always tell whether the focused
 * window actually changed. */
static xcb_window_t last_focused = XCB_NONE;

/* Stores coordinates to warp mouse pointer to if set */
static Rect *warp_to;

/*
 * Describes the X11 state we may modify (map state, position, window stack).
 * There is one entry per container. The state represents the current situation
 * as X11 sees it (with the exception of the order in the state_head CIRCLEQ,
 * which represents the order that will be pushed to X11, while old_state_head
 * represents the current order). It will be updated in x_push_changes().
 *
 */
typedef struct con_state {
    xcb_window_t id;
    bool mapped;
    bool unmap_now;
    bool child_mapped;

    /** The con for which this state is. */
    Con *con;

    /* For reparenting, we have a flag (need_reparent) and the X ID of the old
     * frame this window was in. The latter is necessary because we need to
     * ignore UnmapNotify events (by changing the window event mask). */
    bool need_reparent;
    xcb_window_t old_frame;

    Rect rect;
    Rect window_rect;

    bool initial;

    char *name;

    CIRCLEQ_ENTRY(con_state) state;
    CIRCLEQ_ENTRY(con_state) old_state;
    TAILQ_ENTRY(con_state) initial_mapping_order;
} con_state;

CIRCLEQ_HEAD(state_head, con_state) state_head =
    CIRCLEQ_HEAD_INITIALIZER(state_head);

CIRCLEQ_HEAD(old_state_head, con_state) old_state_head =
    CIRCLEQ_HEAD_INITIALIZER(old_state_head);

TAILQ_HEAD(initial_mapping_head, con_state) initial_mapping_head =
    TAILQ_HEAD_INITIALIZER(initial_mapping_head);

/*
 * Returns the container state for the given frame. This function always
 * returns a container state (otherwise, there is a bug in the code and the
 * container state of a container for which x_con_init() was not called was
 * requested).
 *
 */
static con_state *state_for_frame(xcb_window_t window) {
    con_state *state;
    CIRCLEQ_FOREACH(state, &state_head, state)
    if (state->id == window)
        return state;

    /* TODO: better error handling? */
    ELOG("No state found\n");
    assert(false);
    return NULL;
}

/*
 * Initializes the X11 part for the given container. Called exactly once for
 * every container from con_new().
 *
 */
void x_con_init(Con *con, uint16_t depth) {
    /* TODO: maybe create the window when rendering first? we could then even
     * get the initial geometry right */

    uint32_t mask = 0;
    uint32_t values[5];

    xcb_visualid_t visual = XCB_COPY_FROM_PARENT;
    xcb_colormap_t win_colormap = XCB_NONE;
    if (depth != root_depth && depth != XCB_COPY_FROM_PARENT) {
        /* For custom visuals, we need to create a colormap before creating
         * this window. It will be freed directly after creating the window. */
        visual = get_visualid_by_depth(depth);
        win_colormap = xcb_generate_id(conn);
        xcb_create_colormap_checked(conn, XCB_COLORMAP_ALLOC_NONE, win_colormap, root, visual);

        /* We explicitly set a background color and border color (even though we
         * don’t even have a border) because the X11 server requires us to when
         * using 32 bit color depths, see
         * http://stackoverflow.com/questions/3645632 */
        mask |= XCB_CW_BACK_PIXEL;
        values[0] = root_screen->black_pixel;

        mask |= XCB_CW_BORDER_PIXEL;
        values[1] = root_screen->black_pixel;

        /* our own frames should not be managed */
        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[2] = 1;

        /* see include/xcb.h for the FRAME_EVENT_MASK */
        mask |= XCB_CW_EVENT_MASK;
        values[3] = FRAME_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;

        mask |= XCB_CW_COLORMAP;
        values[4] = win_colormap;
    } else {
        /* our own frames should not be managed */
        mask = XCB_CW_OVERRIDE_REDIRECT;
        values[0] = 1;

        /* see include/xcb.h for the FRAME_EVENT_MASK */
        mask |= XCB_CW_EVENT_MASK;
        values[1] = FRAME_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;

        mask |= XCB_CW_COLORMAP;
        values[2] = colormap;
    }

    Rect dims = {-15, -15, 10, 10};
    con->frame = create_window(conn, dims, depth, visual, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCURSOR_CURSOR_POINTER, false, mask, values);

    if (win_colormap != XCB_NONE)
        xcb_free_colormap(conn, win_colormap);

    struct con_state *state = scalloc(sizeof(struct con_state));
    state->id = con->frame;
    state->mapped = false;
    state->initial = true;
    DLOG("Adding window 0x%08x to lists\n", state->id);
    CIRCLEQ_INSERT_HEAD(&state_head, state, state);
    CIRCLEQ_INSERT_HEAD(&old_state_head, state, old_state);
    TAILQ_INSERT_TAIL(&initial_mapping_head, state, initial_mapping_order);
    DLOG("adding new state for window id 0x%08x\n", state->id);
}

/*
 * Re-initializes the associated X window state for this container. You have
 * to call this when you assign a client to an empty container to ensure that
 * its state gets updated correctly.
 *
 */
void x_reinit(Con *con) {
    struct con_state *state;

    if ((state = state_for_frame(con->frame)) == NULL) {
        ELOG("window state not found\n");
        return;
    }

    DLOG("resetting state %p to initial\n", state);
    state->initial = true;
    state->child_mapped = false;
    state->con = con;
    memset(&(state->window_rect), 0, sizeof(Rect));
}

/*
 * Reparents the child window of the given container (necessary for sticky
 * containers). The reparenting happens in the next call of x_push_changes().
 *
 */
void x_reparent_child(Con *con, Con *old) {
    struct con_state *state;
    if ((state = state_for_frame(con->frame)) == NULL) {
        ELOG("window state for con not found\n");
        return;
    }

    state->need_reparent = true;
    state->old_frame = old->frame;
}

/*
 * Moves a child window from Container src to Container dest.
 *
 */
void x_move_win(Con *src, Con *dest) {
    struct con_state *state_src, *state_dest;

    if ((state_src = state_for_frame(src->frame)) == NULL) {
        ELOG("window state for src not found\n");
        return;
    }

    if ((state_dest = state_for_frame(dest->frame)) == NULL) {
        ELOG("window state for dest not found\n");
        return;
    }

    state_dest->con = state_src->con;
    state_src->con = NULL;

    Rect zero = {0, 0, 0, 0};
    if (memcmp(&(state_dest->window_rect), &(zero), sizeof(Rect)) == 0) {
        memcpy(&(state_dest->window_rect), &(state_src->window_rect), sizeof(Rect));
        DLOG("COPYING RECT\n");
    }
}

/*
 * Kills the window decoration associated with the given container.
 *
 */
void x_con_kill(Con *con) {
    con_state *state;

    xcb_destroy_window(conn, con->frame);
    xcb_free_pixmap(conn, con->pixmap);
    xcb_free_gc(conn, con->pm_gc);
    state = state_for_frame(con->frame);
    CIRCLEQ_REMOVE(&state_head, state, state);
    CIRCLEQ_REMOVE(&old_state_head, state, old_state);
    TAILQ_REMOVE(&initial_mapping_head, state, initial_mapping_order);
    FREE(state->name);
    free(state);

    /* Invalidate focused_id to correctly focus new windows with the same ID */
    focused_id = last_focused = XCB_NONE;
}

/*
 * Returns true if the client supports the given protocol atom (like WM_DELETE_WINDOW)
 *
 */
bool window_supports_protocol(xcb_window_t window, xcb_atom_t atom) {
    xcb_get_property_cookie_t cookie;
    xcb_icccm_get_wm_protocols_reply_t protocols;
    bool result = false;

    cookie = xcb_icccm_get_wm_protocols(conn, window, A_WM_PROTOCOLS);
    if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1)
        return false;

    /* Check if the client’s protocols have the requested atom set */
    for (uint32_t i = 0; i < protocols.atoms_len; i++)
        if (protocols.atoms[i] == atom)
            result = true;

    xcb_icccm_get_wm_protocols_reply_wipe(&protocols);

    return result;
}

/*
 * Kills the given X11 window using WM_DELETE_WINDOW (if supported).
 *
 */
void x_window_kill(xcb_window_t window, kill_window_t kill_window) {
    /* if this window does not support WM_DELETE_WINDOW, we kill it the hard way */
    if (!window_supports_protocol(window, A_WM_DELETE_WINDOW)) {
        if (kill_window == KILL_WINDOW) {
            LOG("Killing specific window 0x%08x\n", window);
            xcb_destroy_window(conn, window);
        } else {
            LOG("Killing the X11 client which owns window 0x%08x\n", window);
            xcb_kill_client(conn, window);
        }
        return;
    }

    /* Every X11 event is 32 bytes long. Therefore, XCB will copy 32 bytes.
     * In order to properly initialize these bytes, we allocate 32 bytes even
     * though we only need less for an xcb_configure_notify_event_t */
    void *event = scalloc(32);
    xcb_client_message_event_t *ev = event;

    ev->response_type = XCB_CLIENT_MESSAGE;
    ev->window = window;
    ev->type = A_WM_PROTOCOLS;
    ev->format = 32;
    ev->data.data32[0] = A_WM_DELETE_WINDOW;
    ev->data.data32[1] = XCB_CURRENT_TIME;

    LOG("Sending WM_DELETE to the client\n");
    xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char *)ev);
    xcb_flush(conn);
    free(event);
}

/*
 * Draws the decoration of the given container onto its parent.
 *
 */
void x_draw_decoration(Con *con) {
    Con *parent = con->parent;
    bool leaf = con_is_leaf(con);

    /* This code needs to run for:
     *  • leaf containers
     *  • non-leaf containers which are in a stacked/tabbed container
     *
     * It does not need to run for:
     *  • direct children of outputs or dockareas
     *  • floating containers (they don’t have a decoration)
     */
    if ((!leaf &&
         parent->layout != L_STACKED &&
         parent->layout != L_TABBED) ||
        parent->type == CT_OUTPUT ||
        parent->type == CT_DOCKAREA ||
        con->type == CT_FLOATING_CON)
        return;

    /* Skip containers whose height is 0 (for example empty dockareas) */
    if (con->rect.height == 0)
        return;

    /* Skip containers whose pixmap has not yet been created (can happen when
     * decoration rendering happens recursively for a window for which
     * x_push_node() was not yet called) */
    if (leaf && con->pixmap == XCB_NONE)
        return;

    /* 1: build deco_params and compare with cache */
    struct deco_render_params *p = scalloc(sizeof(struct deco_render_params));

    /* find out which colors to use */
    if (con->urgent)
        p->color = &config.client.urgent;
    else if (con == focused || con_inside_focused(con))
        p->color = &config.client.focused;
    else if (con == TAILQ_FIRST(&(parent->focus_head)))
        p->color = &config.client.focused_inactive;
    else
        p->color = &config.client.unfocused;

    p->border_style = con_border_style(con);

    Rect *r = &(con->rect);
    Rect *w = &(con->window_rect);
    p->con_rect = (struct width_height){r->width, r->height};
    p->con_window_rect = (struct width_height){w->width, w->height};
    p->con_deco_rect = con->deco_rect;
    p->background = config.client.background;
    p->con_is_leaf = con_is_leaf(con);
    p->parent_layout = con->parent->layout;

    if (con->deco_render_params != NULL &&
        (con->window == NULL || !con->window->name_x_changed) &&
        !parent->pixmap_recreated &&
        !con->pixmap_recreated &&
        !con->mark_changed &&
        memcmp(p, con->deco_render_params, sizeof(struct deco_render_params)) == 0) {
        free(p);
        goto copy_pixmaps;
    }

    Con *next = con;
    while ((next = TAILQ_NEXT(next, nodes))) {
        FREE(next->deco_render_params);
    }

    FREE(con->deco_render_params);
    con->deco_render_params = p;

    if (con->window != NULL && con->window->name_x_changed)
        con->window->name_x_changed = false;

    parent->pixmap_recreated = false;
    con->pixmap_recreated = false;
    con->mark_changed = false;

    /* 2: draw the client.background, but only for the parts around the client_rect */
    if (con->window != NULL) {
        xcb_rectangle_t background[] = {
            /* top area */
            {0, 0, r->width, w->y},
            /* bottom area */
            {0, (w->y + w->height), r->width, r->height - (w->y + w->height)},
            /* left area */
            {0, 0, w->x, r->height},
            /* right area */
            {w->x + w->width, 0, r->width - (w->x + w->width), r->height}};
#if 0
        for (int i = 0; i < 4; i++)
            DLOG("rect is (%d, %d) with %d x %d\n",
                    background[i].x,
                    background[i].y,
                    background[i].width,
                    background[i].height
                );
#endif

        xcb_change_gc(conn, con->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){config.client.background});
        xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, sizeof(background) / sizeof(xcb_rectangle_t), background);
    }

    /* 3: draw a rectangle in border color around the client */
    if (p->border_style != BS_NONE && p->con_is_leaf) {
        /* We might hide some borders adjacent to the screen-edge */
        adjacent_t borders_to_hide = ADJ_NONE;
        borders_to_hide = con_adjacent_borders(con) & config.hide_edge_borders;

        Rect br = con_border_style_rect(con);
#if 0
        DLOG("con->rect spans %d x %d\n", con->rect.width, con->rect.height);
        DLOG("border_rect spans (%d, %d) with %d x %d\n", br.x, br.y, br.width, br.height);
        DLOG("window_rect spans (%d, %d) with %d x %d\n", con->window_rect.x, con->window_rect.y, con->window_rect.width, con->window_rect.height);
#endif

        /* These rectangles represents the border around the child window
         * (left, bottom and right part). We don’t just fill the whole
         * rectangle because some childs are not freely resizable and we want
         * their background color to "shine through". */
        xcb_change_gc(conn, con->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->background});
        if (!(borders_to_hide & ADJ_LEFT_SCREEN_EDGE)) {
            xcb_rectangle_t leftline = {0, 0, br.x, r->height};
            xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, 1, &leftline);
        }
        if (!(borders_to_hide & ADJ_RIGHT_SCREEN_EDGE)) {
            xcb_rectangle_t rightline = {r->width + (br.width + br.x), 0, -(br.width + br.x), r->height};
            xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, 1, &rightline);
        }
        if (!(borders_to_hide & ADJ_LOWER_SCREEN_EDGE)) {
            xcb_rectangle_t bottomline = {br.x, r->height + (br.height + br.y), r->width + br.width, -(br.height + br.y)};
            xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, 1, &bottomline);
        }
        /* 1pixel border needs an additional line at the top */
        if (p->border_style == BS_PIXEL && !(borders_to_hide & ADJ_UPPER_SCREEN_EDGE)) {
            xcb_rectangle_t topline = {br.x, 0, r->width + br.width, br.y};
            xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, 1, &topline);
        }

        /* Highlight the side of the border at which the next window will be
         * opened if we are rendering a single window within a split container
         * (which is undistinguishable from a single window outside a split
         * container otherwise. */
        if (TAILQ_NEXT(con, nodes) == NULL &&
            TAILQ_PREV(con, nodes_head, nodes) == NULL &&
            con->parent->type != CT_FLOATING_CON) {
            xcb_change_gc(conn, con->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->indicator});
            if (p->parent_layout == L_SPLITH)
                xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, 1, (xcb_rectangle_t[]){
                                                                              {r->width + (br.width + br.x), br.y, -(br.width + br.x), r->height + br.height}});
            else if (p->parent_layout == L_SPLITV)
                xcb_poly_fill_rectangle(conn, con->pixmap, con->pm_gc, 1, (xcb_rectangle_t[]){
                                                                              {br.x, r->height + (br.height + br.y), r->width + br.width, -(br.height + br.y)}});
        }
    }

    /* if this is a borderless/1pixel window, we don’t need to render the
     * decoration. */
    if (p->border_style != BS_NORMAL)
        goto copy_pixmaps;

    /* 4: paint the bar */
    xcb_change_gc(conn, parent->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->background});
    xcb_rectangle_t drect = {con->deco_rect.x, con->deco_rect.y, con->deco_rect.width, con->deco_rect.height};
    xcb_poly_fill_rectangle(conn, parent->pixmap, parent->pm_gc, 1, &drect);

    /* 5: draw two unconnected horizontal lines in border color */
    xcb_change_gc(conn, parent->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->border});
    Rect *dr = &(con->deco_rect);
    adjacent_t borders_to_hide = con_adjacent_borders(con) & config.hide_edge_borders;
    int deco_diff_l = borders_to_hide & ADJ_LEFT_SCREEN_EDGE ? 0 : con->current_border_width;
    int deco_diff_r = borders_to_hide & ADJ_RIGHT_SCREEN_EDGE ? 0 : con->current_border_width;
    if (parent->layout == L_TABBED ||
        (parent->layout == L_STACKED && TAILQ_NEXT(con, nodes) != NULL)) {
        deco_diff_l = 0;
        deco_diff_r = 0;
    }
    xcb_segment_t segments[] = {
        {dr->x, dr->y,
         dr->x + dr->width - 1, dr->y},
        {dr->x + deco_diff_l, dr->y + dr->height - 1,
         dr->x - deco_diff_r + dr->width - 1, dr->y + dr->height - 1}};
    xcb_poly_segment(conn, parent->pixmap, parent->pm_gc, 2, segments);

    /* 6: draw the title */
    set_font_colors(parent->pm_gc, p->color->text, p->color->background);
    int text_offset_y = (con->deco_rect.height - config.font.height) / 2;

    struct Window *win = con->window;
    if (win == NULL) {
        /* we have a split container which gets a representation
         * of its children as title
         */
        char *title;
        char *tree = con_get_tree_representation(con);
        sasprintf(&title, "i3: %s", tree);
        free(tree);

        draw_text_ascii(title,
                        parent->pixmap, parent->pm_gc,
                        con->deco_rect.x + 2, con->deco_rect.y + text_offset_y,
                        con->deco_rect.width - 2);
        free(title);

        goto after_title;
    }

    if (win->name == NULL)
        goto copy_pixmaps;

    int indent_level = 0,
        indent_mult = 0;
    Con *il_parent = parent;
    if (il_parent->layout != L_STACKED) {
        while (1) {
            //DLOG("il_parent = %p, layout = %d\n", il_parent, il_parent->layout);
            if (il_parent->layout == L_STACKED)
                indent_level++;
            if (il_parent->type == CT_WORKSPACE || il_parent->type == CT_DOCKAREA || il_parent->type == CT_OUTPUT)
                break;
            il_parent = il_parent->parent;
            indent_mult++;
        }
    }
    //DLOG("indent_level = %d, indent_mult = %d\n", indent_level, indent_mult);
    int indent_px = (indent_level * 5) * indent_mult;

    int mark_width = 0;
    if (config.show_marks && con->mark != NULL && (con->mark)[0] != '_') {
        char *formatted_mark;
        sasprintf(&formatted_mark, "[%s]", con->mark);
        i3String *mark = i3string_from_utf8(formatted_mark);
        FREE(formatted_mark);
        mark_width = predict_text_width(mark);

        draw_text(mark, parent->pixmap, parent->pm_gc,
                  con->deco_rect.x + con->deco_rect.width - mark_width - logical_px(2),
                  con->deco_rect.y + text_offset_y, mark_width);

        I3STRING_FREE(mark);
    }

    draw_text(win->name,
              parent->pixmap, parent->pm_gc,
              con->deco_rect.x + logical_px(2) + indent_px, con->deco_rect.y + text_offset_y,
              con->deco_rect.width - logical_px(2) - indent_px - mark_width - logical_px(2));

after_title:
    /* Since we don’t clip the text at all, it might in some cases be painted
     * on the border pixels on the right side of a window. Therefore, we draw
     * the right border again after rendering the text (and the unconnected
     * lines in border color). */

    /* Draw a 1px separator line before and after every tab, so that tabs can
     * be easily distinguished. */
    if (parent->layout == L_TABBED) {
        xcb_change_gc(conn, parent->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->border});
    } else {
        xcb_change_gc(conn, parent->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->background});
    }
    xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, parent->pixmap, parent->pm_gc, 6,
                  (xcb_point_t[]){
                      {dr->x + dr->width, dr->y},
                      {dr->x + dr->width, dr->y + dr->height},
                      {dr->x + dr->width - 1, dr->y},
                      {dr->x + dr->width - 1, dr->y + dr->height},
                      {dr->x, dr->y + dr->height},
                      {dr->x, dr->y},
                  });

    xcb_change_gc(conn, parent->pm_gc, XCB_GC_FOREGROUND, (uint32_t[]){p->color->border});
    xcb_poly_segment(conn, parent->pixmap, parent->pm_gc, 2, segments);

copy_pixmaps:
    xcb_copy_area(conn, con->pixmap, con->frame, con->pm_gc, 0, 0, 0, 0, con->rect.width, con->rect.height);
}

/*
 * Recursively calls x_draw_decoration. This cannot be done in x_push_node
 * because x_push_node uses focus order to recurse (see the comment above)
 * while drawing the decoration needs to happen in the actual order.
 *
 */
void x_deco_recurse(Con *con) {
    Con *current;
    bool leaf = TAILQ_EMPTY(&(con->nodes_head)) &&
                TAILQ_EMPTY(&(con->floating_head));
    con_state *state = state_for_frame(con->frame);

    if (!leaf) {
        TAILQ_FOREACH(current, &(con->nodes_head), nodes)
        x_deco_recurse(current);

        TAILQ_FOREACH(current, &(con->floating_head), floating_windows)
        x_deco_recurse(current);

        if (state->mapped)
            xcb_copy_area(conn, con->pixmap, con->frame, con->pm_gc, 0, 0, 0, 0, con->rect.width, con->rect.height);
    }

    if ((con->type != CT_ROOT && con->type != CT_OUTPUT) &&
        (!leaf || con->mapped))
        x_draw_decoration(con);
}

/*
 * This function pushes the properties of each node of the layout tree to
 * X11 if they have changed (like the map state, position of the window, …).
 * It recursively traverses all children of the given node.
 *
 */
void x_push_node(Con *con) {
    Con *current;
    con_state *state;
    Rect rect = con->rect;

    //DLOG("Pushing changes for node %p / %s\n", con, con->name);
    state = state_for_frame(con->frame);

    if (state->name != NULL) {
        DLOG("pushing name %s for con %p\n", state->name, con);

        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->frame,
                            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(state->name), state->name);
        FREE(state->name);
    }

    if (con->window == NULL) {
        /* Calculate the height of all window decorations which will be drawn on to
         * this frame. */
        uint32_t max_y = 0, max_height = 0;
        TAILQ_FOREACH(current, &(con->nodes_head), nodes) {
            Rect *dr = &(current->deco_rect);
            if (dr->y >= max_y && dr->height >= max_height) {
                max_y = dr->y;
                max_height = dr->height;
            }
        }
        rect.height = max_y + max_height;
        if (rect.height == 0)
            con->mapped = false;
    }

    /* reparent the child window (when the window was moved due to a sticky
     * container) */
    if (state->need_reparent && con->window != NULL) {
        DLOG("Reparenting child window\n");

        /* Temporarily set the event masks to XCB_NONE so that we won’t get
         * UnmapNotify events (otherwise the handler would close the container).
         * These events are generated automatically when reparenting. */
        uint32_t values[] = {XCB_NONE};
        xcb_change_window_attributes(conn, state->old_frame, XCB_CW_EVENT_MASK, values);
        xcb_change_window_attributes(conn, con->window->id, XCB_CW_EVENT_MASK, values);

        xcb_reparent_window(conn, con->window->id, con->frame, 0, 0);

        values[0] = FRAME_EVENT_MASK;
        xcb_change_window_attributes(conn, state->old_frame, XCB_CW_EVENT_MASK, values);
        values[0] = CHILD_EVENT_MASK;
        xcb_change_window_attributes(conn, con->window->id, XCB_CW_EVENT_MASK, values);

        state->old_frame = XCB_NONE;
        state->need_reparent = false;

        con->ignore_unmap++;
        DLOG("ignore_unmap for reparenting of con %p (win 0x%08x) is now %d\n",
             con, con->window->id, con->ignore_unmap);
    }

    /* The pixmap of a borderless leaf container will not be used except
     * for the titlebar in a stack or tabs (issue #1013). */
    bool is_pixmap_needed = (con->border_style != BS_NONE ||
                             !con_is_leaf(con) ||
                             con->parent->layout == L_STACKED ||
                             con->parent->layout == L_TABBED);

    bool fake_notify = false;
    /* Set new position if rect changed (and if height > 0) or if the pixmap
     * needs to be recreated */
    if ((is_pixmap_needed && con->pixmap == XCB_NONE) || (memcmp(&(state->rect), &rect, sizeof(Rect)) != 0 &&
                                                          rect.height > 0)) {
        /* We first create the new pixmap, then render to it, set it as the
         * background and only afterwards change the window size. This reduces
         * flickering. */

        /* As the pixmap only depends on the size and not on the position, it
         * is enough to check if width/height have changed. Also, we don’t
         * create a pixmap at all when the window is actually not visible
         * (height == 0) or when it is not needed. */
        bool has_rect_changed = (state->rect.width != rect.width || state->rect.height != rect.height);

        /* Check if the container has an unneeded pixmap left over from
         * previously having a border or titlebar. */
        if (!is_pixmap_needed && con->pixmap != XCB_NONE) {
            xcb_free_pixmap(conn, con->pixmap);
            con->pixmap = XCB_NONE;
        }

        if (is_pixmap_needed && (has_rect_changed || con->pixmap == XCB_NONE)) {
            if (con->pixmap == 0) {
                con->pixmap = xcb_generate_id(conn);
                con->pm_gc = xcb_generate_id(conn);
            } else {
                xcb_free_pixmap(conn, con->pixmap);
                xcb_free_gc(conn, con->pm_gc);
            }

            uint16_t win_depth = root_depth;
            if (con->window)
                win_depth = con->window->depth;

            xcb_create_pixmap(conn, win_depth, con->pixmap, con->frame, rect.width, rect.height);

            /* For the graphics context, we disable GraphicsExposure events.
             * Those will be sent when a CopyArea request cannot be fulfilled
             * properly due to parts of the source being unmapped or otherwise
             * unavailable. Since we always copy from pixmaps to windows, this
             * is not a concern for us. */
            uint32_t values[] = {0};
            xcb_create_gc(conn, con->pm_gc, con->pixmap, XCB_GC_GRAPHICS_EXPOSURES, values);

            con->pixmap_recreated = true;

            /* Don’t render the decoration for windows inside a stack which are
             * not visible right now */
            if (!con->parent ||
                con->parent->layout != L_STACKED ||
                TAILQ_FIRST(&(con->parent->focus_head)) == con)
                /* Render the decoration now to make the correct decoration visible
                 * from the very first moment. Later calls will be cached, so this
                 * doesn’t hurt performance. */
                x_deco_recurse(con);
        }

        DLOG("setting rect (%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);
        /* flush to ensure that the following commands are sent in a single
         * buffer and will be processed directly afterwards (the contents of a
         * window get lost when resizing it, therefore we want to provide it as
         * fast as possible) */
        xcb_flush(conn);
        xcb_set_window_rect(conn, con->frame, rect);
        if (con->pixmap != XCB_NONE)
            xcb_copy_area(conn, con->pixmap, con->frame, con->pm_gc, 0, 0, 0, 0, con->rect.width, con->rect.height);
        xcb_flush(conn);

        memcpy(&(state->rect), &rect, sizeof(Rect));
        fake_notify = true;
    }

    /* dito, but for child windows */
    if (con->window != NULL &&
        memcmp(&(state->window_rect), &(con->window_rect), sizeof(Rect)) != 0) {
        DLOG("setting window rect (%d, %d, %d, %d)\n",
             con->window_rect.x, con->window_rect.y, con->window_rect.width, con->window_rect.height);
        xcb_set_window_rect(conn, con->window->id, con->window_rect);
        memcpy(&(state->window_rect), &(con->window_rect), sizeof(Rect));
        fake_notify = true;
    }

    /* Map if map state changed, also ensure that the child window
     * is changed if we are mapped and there is a new, unmapped child window.
     * Unmaps are handled in x_push_node_unmaps(). */
    if ((state->mapped != con->mapped || (con->window != NULL && !state->child_mapped)) &&
        con->mapped) {
        xcb_void_cookie_t cookie;

        if (con->window != NULL) {
            /* Set WM_STATE_NORMAL because GTK applications don’t want to
             * drag & drop if we don’t. Also, xprop(1) needs it. */
            long data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->window->id,
                                A_WM_STATE, A_WM_STATE, 32, 2, data);
        }

        uint32_t values[1];
        if (!state->child_mapped && con->window != NULL) {
            cookie = xcb_map_window(conn, con->window->id);

            /* We are interested in EnterNotifys as soon as the window is
             * mapped */
            values[0] = CHILD_EVENT_MASK;
            xcb_change_window_attributes(conn, con->window->id, XCB_CW_EVENT_MASK, values);
            DLOG("mapping child window (serial %d)\n", cookie.sequence);
            state->child_mapped = true;
        }

        cookie = xcb_map_window(conn, con->frame);

        values[0] = FRAME_EVENT_MASK;
        xcb_change_window_attributes(conn, con->frame, XCB_CW_EVENT_MASK, values);

        /* copy the pixmap contents to the frame window immediately after mapping */
        if (con->pixmap != XCB_NONE)
            xcb_copy_area(conn, con->pixmap, con->frame, con->pm_gc, 0, 0, 0, 0, con->rect.width, con->rect.height);
        xcb_flush(conn);

        DLOG("mapping container %08x (serial %d)\n", con->frame, cookie.sequence);
        state->mapped = con->mapped;
    }

    state->unmap_now = (state->mapped != con->mapped) && !con->mapped;

    if (fake_notify) {
        DLOG("Sending fake configure notify\n");
        fake_absolute_configure_notify(con);
    }

    /* Handle all children and floating windows of this node. We recurse
     * in focus order to display the focused client in a stack first when
     * switching workspaces (reduces flickering). */
    TAILQ_FOREACH(current, &(con->focus_head), focused)
    x_push_node(current);
}

/*
 * Same idea as in x_push_node(), but this function only unmaps windows. It is
 * necessary to split this up to handle new fullscreen clients properly: The
 * new window needs to be mapped and focus needs to be set *before* the
 * underlying windows are unmapped. Otherwise, focus will revert to the
 * PointerRoot and will then be set to the new window, generating unnecessary
 * FocusIn/FocusOut events.
 *
 */
static void x_push_node_unmaps(Con *con) {
    Con *current;
    con_state *state;

    //DLOG("Pushing changes (with unmaps) for node %p / %s\n", con, con->name);
    state = state_for_frame(con->frame);

    /* map/unmap if map state changed, also ensure that the child window
     * is changed if we are mapped *and* in initial state (meaning the
     * container was empty before, but now got a child) */
    if (state->unmap_now) {
        xcb_void_cookie_t cookie;
        if (con->window != NULL) {
            /* Set WM_STATE_WITHDRAWN, it seems like Java apps need it */
            long data[] = {XCB_ICCCM_WM_STATE_WITHDRAWN, XCB_NONE};
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->window->id,
                                A_WM_STATE, A_WM_STATE, 32, 2, data);
        }

        cookie = xcb_unmap_window(conn, con->frame);
        DLOG("unmapping container %p / %s (serial %d)\n", con, con->name, cookie.sequence);
        /* we need to increase ignore_unmap for this container (if it
         * contains a window) and for every window "under" this one which
         * contains a window */
        if (con->window != NULL) {
            con->ignore_unmap++;
            DLOG("ignore_unmap for con %p (frame 0x%08x) now %d\n", con, con->frame, con->ignore_unmap);
        }
        state->mapped = con->mapped;
    }

    /* handle all children and floating windows of this node */
    TAILQ_FOREACH(current, &(con->nodes_head), nodes)
    x_push_node_unmaps(current);

    TAILQ_FOREACH(current, &(con->floating_head), floating_windows)
    x_push_node_unmaps(current);
}

/*
 * Returns true if the given container is currently attached to its parent.
 *
 * TODO: Remove once #1185 has been fixed
 */
static bool is_con_attached(Con *con) {
    if (con->parent == NULL)
        return false;

    Con *current;
    TAILQ_FOREACH(current, &(con->parent->nodes_head), nodes) {
        if (current == con)
            return true;
    }

    return false;
}

/*
 * Pushes all changes (state of each node, see x_push_node() and the window
 * stack) to X11.
 *
 * NOTE: We need to push the stack first so that the windows have the correct
 * stacking order. This is relevant for workspace switching where we map the
 * windows because mapping may generate EnterNotify events. When they are
 * generated in the wrong order, this will cause focus problems when switching
 * workspaces.
 *
 */
void x_push_changes(Con *con) {
    con_state *state;
    xcb_query_pointer_cookie_t pointercookie;

    /* If we need to warp later, we request the pointer position as soon as possible */
    if (warp_to) {
        pointercookie = xcb_query_pointer(conn, root);
    }

    DLOG("-- PUSHING WINDOW STACK --\n");
    //DLOG("Disabling EnterNotify\n");
    uint32_t values[1] = {XCB_NONE};
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        if (state->mapped)
            xcb_change_window_attributes(conn, state->id, XCB_CW_EVENT_MASK, values);
    }
    //DLOG("Done, EnterNotify disabled\n");
    bool order_changed = false;
    bool stacking_changed = false;

    /* count first, necessary to (re)allocate memory for the bottom-to-top
     * stack afterwards */
    int cnt = 0;
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state)
    if (con_has_managed_window(state->con))
        cnt++;

    /* The bottom-to-top window stack of all windows which are managed by i3.
     * Used for x_get_window_stack(). */
    static xcb_window_t *client_list_windows = NULL;
    static int client_list_count = 0;

    if (cnt != client_list_count) {
        client_list_windows = srealloc(client_list_windows, sizeof(xcb_window_t) * cnt);
        client_list_count = cnt;
    }

    xcb_window_t *walk = client_list_windows;

    /* X11 correctly represents the stack if we push it from bottom to top */
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        if (con_has_managed_window(state->con))
            memcpy(walk++, &(state->con->window->id), sizeof(xcb_window_t));

        //DLOG("stack: 0x%08x\n", state->id);
        con_state *prev = CIRCLEQ_PREV(state, state);
        con_state *old_prev = CIRCLEQ_PREV(state, old_state);
        if (prev != old_prev)
            order_changed = true;
        if ((state->initial || order_changed) && prev != CIRCLEQ_END(&state_head)) {
            stacking_changed = true;
            //DLOG("Stacking 0x%08x above 0x%08x\n", prev->id, state->id);
            uint32_t mask = 0;
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            uint32_t values[] = {state->id, XCB_STACK_MODE_ABOVE};

            xcb_configure_window(conn, prev->id, mask, values);
        }
        state->initial = false;
    }

    /* If we re-stacked something (or a new window appeared), we need to update
     * the _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING hints */
    if (stacking_changed) {
        DLOG("Client list changed (%i clients)\n", cnt);
        ewmh_update_client_list_stacking(client_list_windows, client_list_count);

        walk = client_list_windows;

        /* reorder by initial mapping */
        TAILQ_FOREACH(state, &initial_mapping_head, initial_mapping_order) {
            if (con_has_managed_window(state->con))
                *walk++ = state->con->window->id;
        }

        ewmh_update_client_list(client_list_windows, client_list_count);
    }

    DLOG("PUSHING CHANGES\n");
    x_push_node(con);

    if (warp_to) {
        xcb_query_pointer_reply_t *pointerreply = xcb_query_pointer_reply(conn, pointercookie, NULL);
        if (!pointerreply) {
            ELOG("Could not query pointer position, not warping pointer\n");
        } else {
            int mid_x = warp_to->x + (warp_to->width / 2);
            int mid_y = warp_to->y + (warp_to->height / 2);

            Output *current = get_output_containing(pointerreply->root_x, pointerreply->root_y);
            Output *target = get_output_containing(mid_x, mid_y);
            if (current != target) {
                /* Ignore MotionNotify events generated by warping */
                xcb_change_window_attributes(conn, root, XCB_CW_EVENT_MASK, (uint32_t[]){XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT});
                xcb_warp_pointer(conn, XCB_NONE, root, 0, 0, 0, 0, mid_x, mid_y);
                xcb_change_window_attributes(conn, root, XCB_CW_EVENT_MASK, (uint32_t[]){ROOT_EVENT_MASK});
            }
        }
        warp_to = NULL;
    }

    //DLOG("Re-enabling EnterNotify\n");
    values[0] = FRAME_EVENT_MASK;
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        if (state->mapped)
            xcb_change_window_attributes(conn, state->id, XCB_CW_EVENT_MASK, values);
    }
    //DLOG("Done, EnterNotify re-enabled\n");

    x_deco_recurse(con);

    xcb_window_t to_focus = focused->frame;
    if (focused->window != NULL)
        to_focus = focused->window->id;

    if (focused_id != to_focus) {
        if (!focused->mapped) {
            DLOG("Not updating focus (to %p / %s), focused window is not mapped.\n", focused, focused->name);
            /* Invalidate focused_id to correctly focus new windows with the same ID */
            focused_id = XCB_NONE;
        } else {
            if (focused->window != NULL &&
                focused->window->needs_take_focus &&
                focused->window->doesnt_accept_focus) {
                DLOG("Updating focus by sending WM_TAKE_FOCUS to window 0x%08x (focused: %p / %s)\n",
                     to_focus, focused, focused->name);
                send_take_focus(to_focus, last_timestamp);

                ewmh_update_active_window((con_has_managed_window(focused) ? focused->window->id : XCB_WINDOW_NONE));

                if (to_focus != last_focused && is_con_attached(focused))
                    ipc_send_window_event("focus", focused);
            } else {
                DLOG("Updating focus (focused: %p / %s) to X11 window 0x%08x\n", focused, focused->name, to_focus);
                /* We remove XCB_EVENT_MASK_FOCUS_CHANGE from the event mask to get
                 * no focus change events for our own focus changes. We only want
                 * these generated by the clients. */
                if (focused->window != NULL) {
                    values[0] = CHILD_EVENT_MASK & ~(XCB_EVENT_MASK_FOCUS_CHANGE);
                    xcb_change_window_attributes(conn, focused->window->id, XCB_CW_EVENT_MASK, values);
                }
                xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, to_focus, XCB_CURRENT_TIME);
                if (focused->window != NULL) {
                    values[0] = CHILD_EVENT_MASK;
                    xcb_change_window_attributes(conn, focused->window->id, XCB_CW_EVENT_MASK, values);
                }

                ewmh_update_active_window((con_has_managed_window(focused) ? focused->window->id : XCB_WINDOW_NONE));

                if (to_focus != XCB_NONE && to_focus != last_focused && focused->window != NULL && is_con_attached(focused))
                    ipc_send_window_event("focus", focused);
            }

            focused_id = last_focused = to_focus;
        }
    }

    if (focused_id == XCB_NONE) {
        DLOG("Still no window focused, better set focus to the root window\n");
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, root, XCB_CURRENT_TIME);
        ewmh_update_active_window(XCB_WINDOW_NONE);
        focused_id = root;
    }

    xcb_flush(conn);
    DLOG("ENDING CHANGES\n");

    /* Disable EnterWindow events for windows which will be unmapped in
     * x_push_node_unmaps() now. Unmapping windows happens when switching
     * workspaces. We want to avoid getting EnterNotifies during that phase
     * because they would screw up our focus. One of these cases is having a
     * stack with two windows. If the first window is focused and gets
     * unmapped, the second one appears under the cursor and therefore gets an
     * EnterNotify event. */
    values[0] = FRAME_EVENT_MASK & ~XCB_EVENT_MASK_ENTER_WINDOW;
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        if (!state->unmap_now)
            continue;
        xcb_change_window_attributes(conn, state->id, XCB_CW_EVENT_MASK, values);
    }

    /* Push all pending unmaps */
    x_push_node_unmaps(con);

    /* save the current stack as old stack */
    CIRCLEQ_FOREACH(state, &state_head, state) {
        CIRCLEQ_REMOVE(&old_state_head, state, old_state);
        CIRCLEQ_INSERT_TAIL(&old_state_head, state, old_state);
    }
    //CIRCLEQ_FOREACH(state, &old_state_head, old_state) {
    //    DLOG("old stack: 0x%08x\n", state->id);
    //}

    xcb_flush(conn);
}

/*
 * Raises the specified container in the internal stack of X windows. The
 * next call to x_push_changes() will make the change visible in X11.
 *
 */
void x_raise_con(Con *con) {
    con_state *state;
    state = state_for_frame(con->frame);
    //DLOG("raising in new stack: %p / %s / %s / xid %08x\n", con, con->name, con->window ? con->window->name_json : "", state->id);

    CIRCLEQ_REMOVE(&state_head, state, state);
    CIRCLEQ_INSERT_HEAD(&state_head, state, state);
}

/*
 * Sets the WM_NAME property (so, no UTF8, but used only for debugging anyways)
 * of the given name. Used for properly tagging the windows for easily spotting
 * i3 windows in xwininfo -root -all.
 *
 */
void x_set_name(Con *con, const char *name) {
    struct con_state *state;

    if ((state = state_for_frame(con->frame)) == NULL) {
        ELOG("window state not found\n");
        return;
    }

    FREE(state->name);
    state->name = sstrdup(name);
}

/*
 * Set up the I3_SHMLOG_PATH atom.
 *
 */
void update_shmlog_atom() {
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
                        A_I3_SHMLOG_PATH, A_UTF8_STRING, 8,
                        strlen(shmlogname), shmlogname);
}

/*
 * Sets up i3 specific atoms (I3_SOCKET_PATH and I3_CONFIG_PATH)
 *
 */
void x_set_i3_atoms(void) {
    pid_t pid = getpid();
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A_I3_SOCKET_PATH, A_UTF8_STRING, 8,
                        (current_socketpath == NULL ? 0 : strlen(current_socketpath)),
                        current_socketpath);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A_I3_PID, XCB_ATOM_CARDINAL, 32, 1, &pid);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A_I3_CONFIG_PATH, A_UTF8_STRING, 8,
                        strlen(current_configpath), current_configpath);
    update_shmlog_atom();
}

/*
 * Set warp_to coordinates.  This will trigger on the next call to
 * x_push_changes().
 *
 */
void x_set_warp_to(Rect *rect) {
    if (config.mouse_warping != POINTER_WARPING_NONE)
        warp_to = rect;
}

/*
 * Applies the given mask to the event mask of every i3 window decoration X11
 * window. This is useful to disable EnterNotify while resizing so that focus
 * is untouched.
 *
 */
void x_mask_event_mask(uint32_t mask) {
    uint32_t values[] = {FRAME_EVENT_MASK & mask};

    con_state *state;
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        if (state->mapped)
            xcb_change_window_attributes(conn, state->id, XCB_CW_EVENT_MASK, values);
    }
}
