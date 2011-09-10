/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 *
 */

#include "all.h"

/*
 * Creates a new 'regex' struct containing the given pattern and a PCRE
 * compiled regular expression. Also, calls pcre_study because this regex will
 * most likely be used often (like for every new window and on every relevant
 * property change of existing windows).
 *
 * Returns NULL if the pattern could not be compiled into a regular expression
 * (and ELOGs an appropriate error message).
 *
 */
struct regex *regex_new(const char *pattern) {
    const char *error;
    int offset;

    struct regex *re = scalloc(sizeof(struct regex));
    re->pattern = sstrdup(pattern);
    if (!(re->regex = pcre_compile(pattern, 0, &error, &offset, NULL))) {
        ELOG("PCRE regular expression compilation failed at %d: %s",
             offset, error);
        return NULL;
    }
    re->extra = pcre_study(re->regex, 0, &error);
    return re;
}

/*
 * Checks if the given regular expression matches the given input and returns
 * true if it does. In either case, it logs the outcome using LOG(), so it will
 * be visible without any debug loglevel.
 *
 */
bool regex_matches(struct regex *regex, const char *input) {
    int rc;

    /* TODO: is strlen(input) correct for UTF-8 matching? */
    /* TODO: enable UTF-8 */
    if ((rc = pcre_exec(regex->regex, regex->extra, input, strlen(input), 0, 0, NULL, 0)) == 0) {
        LOG("Regular expression \"%s\" matches \"%s\"\n",
            regex->pattern, input);
        return true;
    }

    if (rc == PCRE_ERROR_NOMATCH) {
        LOG("Regular expression \"%s\" does not match \"%s\"\n",
            regex->pattern, input);
        return false;
    }

    /* TODO: handle the other error codes */
    LOG("PCRE error\n");
    return false;
}
