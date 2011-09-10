/*
 * vim:ts=4:sw=4:expandtab
 *
 */
#ifndef _REGEX_H
#define _REGEX_H

/**
 * Creates a new 'regex' struct containing the given pattern and a PCRE
 * compiled regular expression. Also, calls pcre_study because this regex will
 * most likely be used often (like for every new window and on every relevant
 * property change of existing windows).
 *
 * Returns NULL if the pattern could not be compiled into a regular expression
 * (and ELOGs an appropriate error message).
 *
 */
struct regex *regex_new(const char *pattern);

/**
 * Checks if the given regular expression matches the given input and returns
 * true if it does. In either case, it logs the outcome using LOG(), so it will
 * be visible without any debug loglevel.
 *
 */
bool regex_matches(struct regex *regex, const char *input);

#endif
