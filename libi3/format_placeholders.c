/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <stdlib.h>
#include <string.h>

#ifndef CS_STARTS_WITH
#define CS_STARTS_WITH(string, needle) (strncmp((string), (needle), strlen((needle))) == 0)
#endif

/*
 * Replaces occurrences of the defined placeholders in the format string.
 *
 */
char *format_placeholders(char *format, placeholder_t *placeholders, int num) {
    if (format == NULL) {
        return NULL;
    }

    /* We have to first iterate over the string to see how much buffer space
     * we need to allocate. */
    int buffer_len = strlen(format) + 1;
    for (char *walk = format; *walk != '\0'; walk++) {
        for (int i = 0; i < num; i++) {
            if (!CS_STARTS_WITH(walk, placeholders[i].name)) {
                continue;
            }

            buffer_len = buffer_len - strlen(placeholders[i].name) + strlen(placeholders[i].value);
            walk += strlen(placeholders[i].name) - 1;
            break;
        }
    }

    /* Now we can parse the format string. */
    char buffer[buffer_len];
    char *outwalk = buffer;
    for (char *walk = format; *walk != '\0'; walk++) {
        if (*walk != '%') {
            *(outwalk++) = *walk;
            continue;
        }

        bool matched = false;
        for (int i = 0; i < num; i++) {
            if (!CS_STARTS_WITH(walk, placeholders[i].name)) {
                continue;
            }

            matched = true;
            outwalk += sprintf(outwalk, "%s", placeholders[i].value);
            walk += strlen(placeholders[i].name) - 1;
            break;
        }

        if (!matched) {
            *(outwalk++) = *walk;
        }
    }

    *outwalk = '\0';
    return sstrdup(buffer);
}
