/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * util.c: Utility functions, which can be useful everywhere.
 *
 */
#include <sys/wait.h>
#include <stdarg.h>
#include <iconv.h>
#if defined(__OpenBSD__)
#include <sys/cdefs.h>
#endif
#include <fcntl.h>
#include <pwd.h>

#include "all.h"

static iconv_t conversion_descriptor = 0;

int min(int a, int b) {
    return (a < b ? a : b);
}

int max(int a, int b) {
    return (a > b ? a : b);
}

bool rect_contains(Rect rect, uint32_t x, uint32_t y) {
    return (x >= rect.x &&
            x <= (rect.x + rect.width) &&
            y >= rect.y &&
            y <= (rect.y + rect.height));
}

Rect rect_add(Rect a, Rect b) {
    return (Rect){a.x + b.x,
                  a.y + b.y,
                  a.width + b.width,
                  a.height + b.height};
}

/*
 * Updates *destination with new_value and returns true if it was changed or false
 * if it was the same
 *
 */
bool update_if_necessary(uint32_t *destination, const uint32_t new_value) {
    uint32_t old_value = *destination;

    return ((*destination = new_value) != old_value);
}

/*
 * The s* functions (safe) are wrappers around malloc, strdup, …, which exits if one of
 * the called functions returns NULL, meaning that there is no more memory available
 *
 */
void *smalloc(size_t size) {
    void *result = malloc(size);
    exit_if_null(result, "Error: out of memory (malloc(%zd))\n", size);
    return result;
}

void *scalloc(size_t size) {
    void *result = calloc(size, 1);
    exit_if_null(result, "Error: out of memory (calloc(%zd))\n", size);
    return result;
}

void *srealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    exit_if_null(result, "Error: out memory (realloc(%zd))\n", size);
    return result;
}

char *sstrdup(const char *str) {
    char *result = strdup(str);
    exit_if_null(result, "Error: out of memory (strdup())\n");
    return result;
}

/*
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If it
 * does not exist, /bin/sh is used.
 *
 */
void start_application(const char *command) {
    LOG("executing: %s\n", command);
    if (fork() == 0) {
        /* Child process */
        setsid();
        if (fork() == 0) {
            /* Stores the path of the shell */
            static const char *shell = NULL;

            if (shell == NULL)
                if ((shell = getenv("SHELL")) == NULL)
                    shell = "/bin/sh";

            /* This is the child */
            execl(shell, shell, "-c", command, (void*)NULL);
            /* not reached */
        }
        exit(0);
    }
    wait(0);
}

/*
 * Checks a generic cookie for errors and quits with the given message if there
 * was an error.
 *
 */
void check_error(xcb_connection_t *conn, xcb_void_cookie_t cookie, char *err_message) {
    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error != NULL) {
        fprintf(stderr, "ERROR: %s (X error %d)\n", err_message , error->error_code);
        xcb_disconnect(conn);
        exit(-1);
    }
}

/*
 * Converts the given string to UCS-2 big endian for use with
 * xcb_image_text_16(). The amount of real glyphs is stored in real_strlen,
 * a buffer containing the UCS-2 encoded string (16 bit per glyph) is
 * returned. It has to be freed when done.
 *
 */
char *convert_utf8_to_ucs2(char *input, int *real_strlen) {
    size_t input_size = strlen(input) + 1;
    /* UCS-2 consumes exactly two bytes for each glyph */
    int buffer_size = input_size * 2;

    char *buffer = smalloc(buffer_size);
    size_t output_size = buffer_size;
    /* We need to use an additional pointer, because iconv() modifies it */
    char *output = buffer;

    /* We convert the input into UCS-2 big endian */
    if (conversion_descriptor == 0) {
        conversion_descriptor = iconv_open("UCS-2BE", "UTF-8");
        if (conversion_descriptor == 0) {
            fprintf(stderr, "error opening the conversion context\n");
            exit(1);
        }
    }

    /* Get the conversion descriptor back to original state */
    iconv(conversion_descriptor, NULL, NULL, NULL, NULL);

    /* Convert our text */
    int rc = iconv(conversion_descriptor, (void*)&input, &input_size, &output, &output_size);
    if (rc == (size_t)-1) {
        perror("Converting to UCS-2 failed");
        if (real_strlen != NULL)
            *real_strlen = 0;
        return NULL;
    }

    if (real_strlen != NULL)
        *real_strlen = ((buffer_size - output_size) / 2) - 1;

    return buffer;
}

/*
 * This function resolves ~ in pathnames.
 * It may resolve wildcards in the first part of the path, but if no match
 * or multiple matches are found, it just returns a copy of path as given.
 *
 */
char *resolve_tilde(const char *path) {
        static glob_t globbuf;
        char *head, *tail, *result;

        tail = strchr(path, '/');
        head = strndup(path, tail ? tail - path : strlen(path));

        int res = glob(head, GLOB_TILDE, NULL, &globbuf);
        free(head);
        /* no match, or many wildcard matches are bad */
        if (res == GLOB_NOMATCH || globbuf.gl_pathc != 1)
                result = sstrdup(path);
        else if (res != 0) {
                die("glob() failed");
        } else {
                head = globbuf.gl_pathv[0];
                result = scalloc(strlen(head) + (tail ? strlen(tail) : 0) + 1);
                strncpy(result, head, strlen(head));
                strncat(result, tail, strlen(tail));
        }
        globfree(&globbuf);

        return result;
}

/*
 * Checks if the given path exists by calling stat().
 *
 */
bool path_exists(const char *path) {
        struct stat buf;
        return (stat(path, &buf) == 0);
}

/*
 * Goes through the list of arguments (for exec()) and checks if the given argument
 * is present. If not, it copies the arguments (because we cannot realloc it) and
 * appends the given argument.
 *
 */
static char **append_argument(char **original, char *argument) {
    int num_args;
    for (num_args = 0; original[num_args] != NULL; num_args++) {
        DLOG("original argument: \"%s\"\n", original[num_args]);
        /* If the argument is already present we return the original pointer */
        if (strcmp(original[num_args], argument) == 0)
            return original;
    }
    /* Copy the original array */
    char **result = smalloc((num_args+2) * sizeof(char*));
    memcpy(result, original, num_args * sizeof(char*));
    result[num_args] = argument;
    result[num_args+1] = NULL;

    return result;
}

/*
 * Returns the name of a temporary file with the specified prefix.
 *
 */
char *get_process_filename(const char *prefix)
{
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "unknown";
    char *filename;
    int res = asprintf(&filename, "/tmp/%s-%s.%d", prefix, username, getpid());
    if (res == -1) {
        perror("asprintf()");
        return NULL;
    }
    else {
        return filename;
    }
}

#define y(x, ...) yajl_gen_ ## x (gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(gen, (unsigned char*)str, strlen(str))

char *store_restart_layout() {
    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = yajl_gen_alloc(NULL, NULL);

    dump_node(gen, croot, true);

    setlocale(LC_NUMERIC, "");

    const unsigned char *payload;
    unsigned int length;
    y(get_buf, &payload, &length);

    /* create a temporary file if one hasn't been specified, or just
     * resolve the tildes in the specified path */
    char *filename;
    if (config.restart_state_path == NULL) {
        filename = get_process_filename("i3-restart-state");
        if (!filename)
            return NULL;
    } else {
        filename = resolve_tilde(config.restart_state_path);
    }

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open()");
        free(filename);
        return NULL;
    }

    int written = 0;
    while (written < length) {
        int n = write(fd, payload + written, length - written);
        /* TODO: correct error-handling */
        if (n == -1) {
            perror("write()");
            free(filename);
            return NULL;
        }
        if (n == 0) {
            printf("write == 0?\n");
            free(filename);
            return NULL;
        }
        written += n;
        printf("written: %d of %d\n", written, length);
    }
    close(fd);

    printf("layout: %.*s\n", length, payload);

    y(free);

    return filename;
}

/*
 * Restart i3 in-place
 * appends -a to argument list to disable autostart
 *
 */
void i3_restart(bool forget_layout) {
    char *restart_filename = forget_layout ? NULL : store_restart_layout();

    restore_geometry();

    ipc_shutdown();

    LOG("restarting \"%s\"...\n", start_argv[0]);
    /* make sure -a is in the argument list or append it */
    start_argv = append_argument(start_argv, "-a");

    /* replace -r <file> so that the layout is restored */
    if (restart_filename != NULL) {
        /* create the new argv */
        int num_args;
        for (num_args = 0; start_argv[num_args] != NULL; num_args++);
        char **new_argv = scalloc((num_args + 3) * sizeof(char*));

        /* copy the arguments, but skip the ones we'll replace */
        int write_index = 0;
        bool skip_next = false;
        for (int i = 0; i < num_args; ++i) {
            if (skip_next)
                skip_next = false;
            else if (!strcmp(start_argv[i], "-r"))
                skip_next = true;
            else
                new_argv[write_index++] = start_argv[i];
        }

        /* add the arguments we'll replace */
        new_argv[write_index++] = "--restart";
        new_argv[write_index++] = restart_filename;

        /* swap the argvs */
        start_argv = new_argv;
    }

    execvp(start_argv[0], start_argv);
    /* not reached */
}

#if 0

#if defined(__OpenBSD__)

/*
 * Taken from FreeBSD
 * Find the first occurrence of the byte string s in byte string l.
 *
 */
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len) {
        register char *cur, *last;
        const char *cl = (const char *)l;
        const char *cs = (const char *)s;

        /* we need something to compare */
        if (l_len == 0 || s_len == 0)
                return NULL;

        /* "s" must be smaller or equal to "l" */
        if (l_len < s_len)
                return NULL;

        /* special case where s_len == 1 */
        if (s_len == 1)
                return memchr(l, (int)*cs, l_len);

        /* the last position where its possible to find "s" in "l" */
        last = (char *)cl + l_len - s_len;

        for (cur = (char *)cl; cur <= last; cur++)
                if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
                        return cur;

        return NULL;
}

#endif
#endif
