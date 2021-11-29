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
    int errorcode;
    PCRE2_SIZE offset;

    struct regex *re = scalloc(1, sizeof(struct regex));
    re->pattern = sstrdup(pattern);
    uint32_t options = PCRE2_UTF;
    /* We use PCRE_UCP so that \B, \b, \D, \d, \S, \s, \W, \w and some POSIX
     * character classes play nicely with Unicode */
    options |= PCRE2_UCP;
    if (!(re->regex = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, options, &errorcode, &offset, NULL))) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
        ELOG("PCRE regular expression compilation failed at %lu: %s\n",
             offset, buffer);
        regex_free(re);
        return NULL;
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
    FREE(regex);
}

/*
 * Checks if the given regular expression matches the given input and returns
 * true if it does. In either case, it logs the outcome using LOG(), so it will
 * be visible without debug logging.
 *
 */
bool regex_matches(struct regex *regex, const char *input) {
    pcre2_match_data *match_data;
    int rc;

    match_data = pcre2_match_data_create_from_pattern(regex->regex, NULL);

    /* We use strlen() because pcre_exec() expects the length of the input
     * string in bytes */
    rc = pcre2_match(regex->regex, (PCRE2_SPTR)input, strlen(input), 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);
    if (rc > 0) {
        LOG("Regular expression \"%s\" matches \"%s\"\n",
            regex->pattern, input);
        return true;
    }

    if (rc == PCRE2_ERROR_NOMATCH) {
        LOG("Regular expression \"%s\" does not match \"%s\"\n",
            regex->pattern, input);
        return false;
    }

    ELOG("PCRE error %d while trying to use regular expression \"%s\" on input \"%s\", see pcreapi(3)\n",
         rc, regex->pattern, input);
    return false;
}
