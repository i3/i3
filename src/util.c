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

#define y(x, ...) yajl_gen_ ## x (gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(gen, (unsigned char*)str, strlen(str))

void store_restart_layout() {
    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = yajl_gen_alloc(NULL, NULL);

    dump_node(gen, croot, true);

    setlocale(LC_NUMERIC, "");

    const unsigned char *payload;
    unsigned int length;
    y(get_buf, &payload, &length);

    char *globbed = resolve_tilde(config.restart_state_path);
    int fd = open(globbed, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    free(globbed);
    if (fd == -1) {
        perror("open()");
        return;
    }

    int written = 0;
    while (written < length) {
        int n = write(fd, payload + written, length - written);
        /* TODO: correct error-handling */
        if (n == -1) {
            perror("write()");
            return;
        }
        if (n == 0) {
            printf("write == 0?\n");
            return;
        }
        written += n;
        printf("written: %d of %d\n", written, length);
    }
    close(fd);

    printf("layout: %.*s\n", length, payload);

    y(free);
}

/*
 * Restart i3 in-place
 * appends -a to argument list to disable autostart
 *
 */
void i3_restart() {
    store_restart_layout();
    restore_geometry();

    ipc_shutdown();

    LOG("restarting \"%s\"...\n", start_argv[0]);
    /* make sure -a is in the argument list or append it */
    start_argv = append_argument(start_argv, "-a");

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
