#ifndef _MATCH_H
#define _MATCH_H

/*
 * Initializes the Match data structure. This function is necessary because the
 * members representing boolean values (like dock) need to be initialized with
 * -1 instead of 0.
 *
 */
void match_init(Match *match);

/**
 * Check if a match is empty. This is necessary while parsing commands to see
 * whether the user specified a match at all.
 *
 */
bool match_is_empty(Match *match);

/**
 * Check if a match data structure matches the given window.
 *
 */
bool match_matches_window(Match *match, i3Window *window);

#endif
