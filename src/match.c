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

/*
 * Initializes the Match data structure. This function is necessary because the
 * members representing boolean values (like dock) need to be initialized with
 * -1 instead of 0.
 *
 */
void match_init(Match *match) {
    memset(match, 0, sizeof(Match));
    match->dock = -1;
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
            match->id == XCB_NONE &&
            match->con_id == NULL &&
            match->dock == -1 &&
            match->floating == M_ANY);
}

/*
 * Check if a match data structure matches the given window.
 *
 */
bool match_matches_window(Match *match, i3Window *window) {
    /* TODO: pcre, full matching, … */
    if (match->class != NULL && window->class_class != NULL && strcasecmp(match->class, window->class_class) == 0) {
        LOG("match made by window class (%s)\n", window->class_class);
        return true;
    }

    if (match->instance != NULL && window->class_instance != NULL && strcasecmp(match->instance, window->class_instance) == 0) {
        LOG("match made by window instance (%s)\n", window->class_instance);
        return true;
    }

    if (match->id != XCB_NONE && window->id == match->id) {
        LOG("match made by window id (%d)\n", window->id);
        return true;
    }

    /* TODO: pcre match */
    if (match->title != NULL && window->name_json != NULL && strcasecmp(match->title, window->name_json) == 0) {
        LOG("match made by title (%s)\n", window->name_json);
        return true;
    }

    LOG("match->dock = %d, window->dock = %d\n", match->dock, window->dock);
    if (match->dock != -1 &&
        ((window->dock == W_DOCK_TOP && match->dock == M_DOCK_TOP) ||
         (window->dock == W_DOCK_BOTTOM && match->dock == M_DOCK_BOTTOM) ||
         ((window->dock == W_DOCK_TOP || window->dock == W_DOCK_BOTTOM) &&
          match->dock == M_DOCK_ANY) ||
         (window->dock == W_NODOCK && match->dock == M_NODOCK))) {
        LOG("match made by dock\n");
        return true;
    }

    LOG("window %d (%s) could not be matched\n", window->id, window->class_class);

    return false;
}

/*
 * Returns the first match in 'assignments' that matches the given window.
 *
 */
Match *match_by_assignment(i3Window *window) {
    Match *match;

    TAILQ_FOREACH(match, &assignments, assignments) {
        if (!match_matches_window(match, window))
            continue;
        DLOG("got a matching assignment (to %s)\n", match->target_ws);
        return match;
    }

    return NULL;
}
