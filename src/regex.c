#undef I3__FILE__
#define I3__FILE__ "regex.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * regex.c: Interface to libPCRE (perl compatible regular expressions).
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
    int errorcode, offset;

    struct regex *re = scalloc(sizeof(struct regex));
    re->pattern = sstrdup(pattern);
    int options = PCRE_UTF8;
#ifdef PCRE_HAS_UCP
    /* We use PCRE_UCP so that \B, \b, \D, \d, \S, \s, \W, \w and some POSIX
     * character classes play nicely with Unicode */
    options |= PCRE_UCP;
#endif
    while (!(re->regex = pcre_compile2(pattern, options, &errorcode, &error, &offset, NULL))) {
        /* If the error is that PCRE was not compiled with UTF-8 support we
         * disable it and try again */
        if (errorcode == 32) {
            options &= ~PCRE_UTF8;
            continue;
        }
        ELOG("PCRE regular expression compilation failed at %d: %s\n",
             offset, error);
        return NULL;
    }
    re->extra = pcre_study(re->regex, 0, &error);
    /* If an error happened, we print the error message, but continue.
     * Studying the regular expression leads to faster matching, but it’s not
     * absolutely necessary. */
    if (error) {
        ELOG("PCRE regular expression studying failed: %s\n", error);
    }
    return re;
}

/*
 * Frees the given regular expression. It must not be used afterwards!
 *
 */
void regex_free(struct regex *regex) {
    if (!regex)
        return;
    FREE(regex->pattern);
    FREE(regex->regex);
    FREE(regex->extra);
}

/*
 * Checks if the given regular expression matches the given input and returns
 * true if it does. In either case, it logs the outcome using LOG(), so it will
 * be visible without debug logging.
 *
 */
bool regex_matches(struct regex *regex, const char *input) {
    int rc;

    /* We use strlen() because pcre_exec() expects the length of the input
     * string in bytes */
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

    ELOG("PCRE error %d while trying to use regular expression \"%s\" on input \"%s\", see pcreapi(3)\n",
         rc, regex->pattern, input);
    return false;
}
