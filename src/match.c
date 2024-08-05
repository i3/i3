/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * A "match" is a data structure which acts like a mask or expression to match
 * certain windows or not. For example, when using commands, you can specify a
 * command like this: [title="*Firefox*"] kill. The title member of the match
 * data structure will then be filled and i3 will check each window using
 * match_matches_window() to find the windows affected by this command.
 *
 */
#include "all.h"

/* From sys/time.h, not sure if it’s available on all systems. */
#define _i3_timercmp(a, b, CMP) \
    (((a).tv_sec == (b).tv_sec) ? ((a).tv_usec CMP(b).tv_usec) : ((a).tv_sec CMP(b).tv_sec))

/*
 * Initializes the Match data structure. This function is necessary because the
 * members representing boolean values (like dock) need to be initialized with
 * -1 instead of 0.
 *
 */
void match_init(Match *match) {
    memset(match, 0, sizeof(Match));
    match->urgent = U_DONTCHECK;
    match->window_mode = WM_ANY;
    /* we use this as the placeholder value for "not set". */
    match->window_type = UINT32_MAX;
}

/*
 * Check if a match is empty. This is necessary while parsing commands to see
 * whether the user specified a match at all.
 *
 */
bool match_is_empty(Match *match) {
    /* we cannot simply use memcmp() because the structure is part of a
     * TAILQ and I don’t want to start with things like assuming that the
     * last member of a struct really is at the end in memory… */
    return (match->title == NULL &&
            match->mark == NULL &&
            match->application == NULL &&
            match->class == NULL &&
            match->instance == NULL &&
            match->window_role == NULL &&
            match->workspace == NULL &&
            match->machine == NULL &&
            match->urgent == U_DONTCHECK &&
            match->id == XCB_NONE &&
            match->window_type == UINT32_MAX &&
            match->con_id == NULL &&
            match->dock == M_NODOCK &&
            match->window_mode == WM_ANY &&
            match->match_all_windows == false);
}

/*
 * Copies the data of a match from src to dest.
 *
 */
void match_copy(Match *dest, Match *src) {
    memcpy(dest, src, sizeof(Match));

/* The DUPLICATE_REGEX macro creates a new regular expression from the
 * ->pattern of the old one. It therefore does use a little more memory then
 *  with a refcounting system, but it’s easier this way. */
#define DUPLICATE_REGEX(field)                            \
    do {                                                  \
        if (src->field != NULL)                           \
            dest->field = regex_new(src->field->pattern); \
    } while (0)

    DUPLICATE_REGEX(title);
    DUPLICATE_REGEX(mark);
    DUPLICATE_REGEX(application);
    DUPLICATE_REGEX(class);
    DUPLICATE_REGEX(instance);
    DUPLICATE_REGEX(window_role);
    DUPLICATE_REGEX(workspace);
    DUPLICATE_REGEX(machine);
}

/*
 * Check if a match data structure matches the given window.
 *
 */
bool match_matches_window(Match *match, i3Window *window) {
    LOG("Checking window 0x%08x (class %s)\n", window->id, window->class_class);

#define GET_FIELD_str(field) (field)
#define GET_FIELD_i3string(field) (i3string_as_utf8(field))
#define CHECK_WINDOW_FIELD(match_field, window_field, type)                                       \
    do {                                                                                          \
        if (match->match_field != NULL) {                                                         \
            const char *window_field_str = window->window_field == NULL                           \
                                               ? ""                                               \
                                               : GET_FIELD_##type(window->window_field);          \
            if (strcmp(match->match_field->pattern, "__focused__") == 0 &&                        \
                focused && focused->window && focused->window->window_field &&                    \
                strcmp(window_field_str, GET_FIELD_##type(focused->window->window_field)) == 0) { \
                LOG("window " #match_field " matches focused window\n");                          \
            } else if (regex_matches(match->match_field, window_field_str)) {                     \
                LOG("window " #match_field " matches (%s)\n", window_field_str);                  \
            } else {                                                                              \
                return false;                                                                     \
            }                                                                                     \
        }                                                                                         \
    } while (0)

    CHECK_WINDOW_FIELD(class, class_class, str);
    CHECK_WINDOW_FIELD(instance, class_instance, str);

    if (match->id != XCB_NONE) {
        if (window->id == match->id) {
            LOG("match made by window id (%d)\n", window->id);
        } else {
            LOG("window id does not match\n");
            return false;
        }
    }

    CHECK_WINDOW_FIELD(title, name, i3string);
    CHECK_WINDOW_FIELD(window_role, role, str);

    if (match->window_type != UINT32_MAX) {
        if (window->window_type == match->window_type) {
            LOG("window_type matches (%i)\n", match->window_type);
        } else {
            return false;
        }
    }

    CHECK_WINDOW_FIELD(machine, machine, str);

    Con *con = NULL;
    if (match->urgent == U_LATEST) {
        /* if the window isn't urgent, no sense in searching */
        if (window->urgent.tv_sec == 0) {
            return false;
        }
        /* if we find a window that is newer than this one, bail */
        TAILQ_FOREACH (con, &all_cons, all_cons) {
            if ((con->window != NULL) &&
                _i3_timercmp(con->window->urgent, window->urgent, >)) {
                return false;
            }
        }
        LOG("urgent matches latest\n");
    }

    if (match->urgent == U_OLDEST) {
        /* if the window isn't urgent, no sense in searching */
        if (window->urgent.tv_sec == 0) {
            return false;
        }
        /* if we find a window that is older than this one (and not 0), bail */
        TAILQ_FOREACH (con, &all_cons, all_cons) {
            if ((con->window != NULL) &&
                (con->window->urgent.tv_sec != 0) &&
                _i3_timercmp(con->window->urgent, window->urgent, <)) {
                return false;
            }
        }
        LOG("urgent matches oldest\n");
    }

    if (match->workspace != NULL) {
        if ((con = con_by_window_id(window->id)) == NULL) {
            return false;
        }

        Con *ws = con_get_workspace(con);
        if (ws == NULL) {
            return false;
        }

        if (strcmp(match->workspace->pattern, "__focused__") == 0 &&
            strcmp(ws->name, con_get_workspace(focused)->name) == 0) {
            LOG("workspace matches focused workspace\n");
        } else if (regex_matches(match->workspace, ws->name)) {
            LOG("workspace matches (%s)\n", ws->name);
        } else {
            return false;
        }
    }

    if (match->dock != M_DONTCHECK) {
        if ((window->dock == W_DOCK_TOP && match->dock == M_DOCK_TOP) ||
            (window->dock == W_DOCK_BOTTOM && match->dock == M_DOCK_BOTTOM) ||
            ((window->dock == W_DOCK_TOP || window->dock == W_DOCK_BOTTOM) &&
             match->dock == M_DOCK_ANY) ||
            (window->dock == W_NODOCK && match->dock == M_NODOCK)) {
            LOG("dock status matches\n");
        } else {
            LOG("dock status does not match\n");
            return false;
        }
    }

    if (match->mark != NULL) {
        if ((con = con_by_window_id(window->id)) == NULL) {
            return false;
        }

        bool matched = false;
        mark_t *mark;
        TAILQ_FOREACH (mark, &(con->marks_head), marks) {
            if (regex_matches(match->mark, mark->name)) {
                matched = true;
                break;
            }
        }

        if (matched) {
            LOG("mark matches\n");
        } else {
            LOG("mark does not match\n");
            return false;
        }
    }

    if (match->window_mode != WM_ANY) {
        if ((con = con_by_window_id(window->id)) == NULL) {
            return false;
        }

        switch (match->window_mode) {
            case WM_TILING_AUTO:
                if (con->floating != FLOATING_AUTO_OFF) {
                    return false;
                }
                break;
            case WM_TILING_USER:
                if (con->floating != FLOATING_USER_OFF) {
                    return false;
                }
                break;
            case WM_TILING:
                if (con_inside_floating(con) != NULL) {
                    return false;
                }
                break;
            case WM_FLOATING_AUTO:
                if (con->floating != FLOATING_AUTO_ON) {
                    return false;
                }
                break;
            case WM_FLOATING_USER:
                if (con->floating != FLOATING_USER_ON) {
                    return false;
                }
                break;
            case WM_FLOATING:
                if (con_inside_floating(con) == NULL) {
                    return false;
                }
                break;
            case WM_ANY:
                assert(false);
        }

        LOG("window_mode matches\n");
    }

    /* NOTE: See the comment regarding 'all' in match_parse_property()
     * for an explanation of why match_all_windows isn't explicitly
     * checked. */

    return true;
}

/*
 * Frees the given match. It must not be used afterwards!
 *
 */
void match_free(Match *match) {
    FREE(match->error);
    regex_free(match->title);
    regex_free(match->application);
    regex_free(match->class);
    regex_free(match->instance);
    regex_free(match->mark);
    regex_free(match->window_role);
    regex_free(match->workspace);
    regex_free(match->machine);
}

/*
 * Interprets a ctype=cvalue pair and adds it to the given match specification.
 *
 */
void match_parse_property(Match *match, const char *ctype, const char *cvalue) {
    assert(match != NULL);
    DLOG("ctype=*%s*, cvalue=*%s*\n", ctype, cvalue);

    if (strcmp(ctype, "class") == 0) {
        regex_free(match->class);
        match->class = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "instance") == 0) {
        regex_free(match->instance);
        match->instance = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "window_role") == 0) {
        regex_free(match->window_role);
        match->window_role = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "con_id") == 0) {
        if (strcmp(cvalue, "__focused__") == 0) {
            match->con_id = focused;
            return;
        }

        long parsed;
        if (!parse_long(cvalue, &parsed, 0)) {
            ELOG("Could not parse con id \"%s\"\n", cvalue);
            match->error = sstrdup("invalid con_id");
        } else {
            match->con_id = (Con *)parsed;
            DLOG("id as int = %p\n", match->con_id);
        }
        return;
    }

    if (strcmp(ctype, "id") == 0) {
        long parsed;
        if (!parse_long(cvalue, &parsed, 0)) {
            ELOG("Could not parse window id \"%s\"\n", cvalue);
            match->error = sstrdup("invalid id");
        } else {
            match->id = parsed;
            DLOG("window id as int = %d\n", match->id);
        }
        return;
    }

    if (strcmp(ctype, "window_type") == 0) {
        if (strcasecmp(cvalue, "normal") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_NORMAL;
        } else if (strcasecmp(cvalue, "dialog") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_DIALOG;
        } else if (strcasecmp(cvalue, "utility") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_UTILITY;
        } else if (strcasecmp(cvalue, "toolbar") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_TOOLBAR;
        } else if (strcasecmp(cvalue, "splash") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_SPLASH;
        } else if (strcasecmp(cvalue, "menu") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_MENU;
        } else if (strcasecmp(cvalue, "dropdown_menu") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
        } else if (strcasecmp(cvalue, "popup_menu") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_POPUP_MENU;
        } else if (strcasecmp(cvalue, "tooltip") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_TOOLTIP;
        } else if (strcasecmp(cvalue, "notification") == 0) {
            match->window_type = A__NET_WM_WINDOW_TYPE_NOTIFICATION;
        } else {
            ELOG("unknown window_type value \"%s\"\n", cvalue);
            match->error = sstrdup("unknown window_type value");
        }

        return;
    }

    if (strcmp(ctype, "con_mark") == 0) {
        regex_free(match->mark);
        match->mark = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "title") == 0) {
        regex_free(match->title);
        match->title = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "urgent") == 0) {
        if (strcasecmp(cvalue, "latest") == 0 ||
            strcasecmp(cvalue, "newest") == 0 ||
            strcasecmp(cvalue, "recent") == 0 ||
            strcasecmp(cvalue, "last") == 0) {
            match->urgent = U_LATEST;
        } else if (strcasecmp(cvalue, "oldest") == 0 ||
                   strcasecmp(cvalue, "first") == 0) {
            match->urgent = U_OLDEST;
        }
        return;
    }

    if (strcmp(ctype, "workspace") == 0) {
        regex_free(match->workspace);
        match->workspace = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "machine") == 0) {
        regex_free(match->machine);
        match->machine = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "tiling") == 0) {
        match->window_mode = WM_TILING;
        return;
    }

    if (strcmp(ctype, "tiling_from") == 0 &&
        cvalue != NULL &&
        strcmp(cvalue, "auto") == 0) {
        match->window_mode = WM_TILING_AUTO;
        return;
    }

    if (strcmp(ctype, "tiling_from") == 0 &&
        cvalue != NULL &&
        strcmp(cvalue, "user") == 0) {
        match->window_mode = WM_TILING_USER;
        return;
    }

    if (strcmp(ctype, "floating") == 0) {
        match->window_mode = WM_FLOATING;
        return;
    }

    if (strcmp(ctype, "floating_from") == 0 &&
        cvalue != NULL &&
        strcmp(cvalue, "auto") == 0) {
        match->window_mode = WM_FLOATING_AUTO;
        return;
    }

    if (strcmp(ctype, "floating_from") == 0 &&
        cvalue != NULL &&
        strcmp(cvalue, "user") == 0) {
        match->window_mode = WM_FLOATING_USER;
        return;
    }

    /* match_matches_window() only checks negatively, so match_all_windows
     * won't actually be used there, but that's OK because if no negative
     * match is found (e.g. because of a more restrictive criterion) the
     * return value of match_matches_window() is true.
     * Setting it here only serves to cause match_is_empty() to return false,
     * otherwise empty criteria rules apply, and that's not what we want. */
    if (strcmp(ctype, "all") == 0) {
        match->match_all_windows = true;
        return;
    }

    ELOG("Unknown criterion: %s\n", ctype);
}
