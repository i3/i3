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
#pragma once

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
 * Copies the data of a match from src to dest.
 *
 */
void match_copy(Match *dest, Match *src);

/**
 * Check if a match data structure matches the given window.
 *
 */
bool match_matches_window(Match *match, i3Window *window);

/**
 * Check if a match data structure matches the workspace of the given
 * container.
 *
 */
bool match_matches_workspace(Match *match, Con *con);

/**
 * Frees the given match. It must not be used afterwards!
 *
 */
void match_free(Match *match);
