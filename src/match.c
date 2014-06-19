#undef I3__FILE__
#define I3__FILE__ "match.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
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
    match->dock = -1;
    match->urgent = U_DONTCHECK;
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
            match->urgent == U_DONTCHECK &&
            match->id == XCB_NONE &&
            match->con_id == NULL &&
            match->dock == -1 &&
            match->floating == M_ANY);
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
}

/*
 * Check if a match data structure matches the given window.
 *
 */
bool match_matches_window(Match *match, i3Window *window) {
    LOG("Checking window 0x%08x (class %s)\n", window->id, window->class_class);

    if (match->class != NULL) {
        if (window->class_class != NULL &&
            regex_matches(match->class, window->class_class)) {
            LOG("window class matches (%s)\n", window->class_class);
        } else {
            return false;
        }
    }

    if (match->instance != NULL) {
        if (window->class_instance != NULL &&
            regex_matches(match->instance, window->class_instance)) {
            LOG("window instance matches (%s)\n", window->class_instance);
        } else {
            return false;
        }
    }

    if (match->id != XCB_NONE) {
        if (window->id == match->id) {
            LOG("match made by window id (%d)\n", window->id);
        } else {
            LOG("window id does not match\n");
            return false;
        }
    }

    if (match->title != NULL) {
        if (window->name != NULL &&
            regex_matches(match->title, i3string_as_utf8(window->name))) {
            LOG("title matches (%s)\n", i3string_as_utf8(window->name));
        } else {
            return false;
        }
    }

    if (match->window_role != NULL) {
        if (window->role != NULL &&
            regex_matches(match->window_role, window->role)) {
            LOG("window_role matches (%s)\n", window->role);
        } else {
            return false;
        }
    }

    Con *con = NULL;
    if (match->urgent == U_LATEST) {
        /* if the window isn't urgent, no sense in searching */
        if (window->urgent.tv_sec == 0) {
            return false;
        }
        /* if we find a window that is newer than this one, bail */
        TAILQ_FOREACH(con, &all_cons, all_cons) {
            if ((con->window != NULL) &&
                _i3_timercmp(con->window->urgent, window->urgent, > )) {
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
        TAILQ_FOREACH(con, &all_cons, all_cons) {
            if ((con->window != NULL) &&
                (con->window->urgent.tv_sec != 0) &&
                _i3_timercmp(con->window->urgent, window->urgent, < )) {
                return false;
            }
        }
        LOG("urgent matches oldest\n");
    }

    if (match->dock != -1) {
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

    /* We don’t check the mark because this function is not even called when
     * the mark would have matched - it is checked in cmdparse.y itself */
    if (match->mark != NULL) {
        LOG("mark does not match\n");
        return false;
    }

    return true;
}

/*
 * Frees the given match. It must not be used afterwards!
 *
 */
void match_free(Match *match) {
    /* First step: free the regex fields / patterns */
    regex_free(match->title);
    regex_free(match->application);
    regex_free(match->class);
    regex_free(match->instance);
    regex_free(match->mark);
    regex_free(match->window_role);

    /* Second step: free the regex helper struct itself */
    FREE(match->title);
    FREE(match->application);
    FREE(match->class);
    FREE(match->instance);
    FREE(match->mark);
    FREE(match->window_role);
}
