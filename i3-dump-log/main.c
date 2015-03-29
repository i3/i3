/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-dump-log/main.c: Dumps the i3 SHM log to stdout.
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "libi3.h"
#include "shmlog.h"
#include <i3/ipc.h>

static uint32_t offset_next_write,
    wrap_count;

static i3_shmlog_header *header;
static char *logbuffer,
    *walk;

static int check_for_wrap(void) {
    if (wrap_count == header->wrap_count)
        return 0;

    /* The log wrapped. Print the remaining content and reset walk to the top
     * of the log. */
    wrap_count = header->wrap_count;
    const int len = (logbuffer + header->offset_last_wrap) - walk;
    swrite(STDOUT_FILENO, walk, len);
    walk = logbuffer + sizeof(i3_shmlog_header);
    return 1;
}

static void print_till_end(void) {
    check_for_wrap();
    const int len = (logbuffer + header->offset_next_write) - walk;
    swrite(STDOUT_FILENO, walk, len);
    walk += len;
}

int main(int argc, char *argv[]) {
    int o, option_index = 0;
    bool verbose = false,
         follow = false;

    static struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"verbose", no_argument, 0, 'V'},
        {"follow", no_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    char *options_string = "s:vfVh";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        if (o == 'v') {
            printf("i3-dump-log " I3_VERSION "\n");
            return 0;
        } else if (o == 'V') {
            verbose = true;
        } else if (o == 'f') {
            follow = true;
        } else if (o == 'h') {
            printf("i3-dump-log " I3_VERSION "\n");
            printf("i3-dump-log [-f] [-s <socket>]\n");
            return 0;
        }
    }

    char *shmname = root_atom_contents("I3_SHMLOG_PATH", NULL, 0);
    if (shmname == NULL) {
        /* Something failed. Let’s invest a little effort to find out what it
         * is. This is hugely helpful for users who want to debug i3 but are
         * not used to the procedure yet. */
        xcb_connection_t *conn;
        int screen;
        if ((conn = xcb_connect(NULL, &screen)) == NULL ||
            xcb_connection_has_error(conn)) {
            fprintf(stderr, "i3-dump-log: ERROR: Cannot connect to X11.\n\n");
            if (getenv("DISPLAY") == NULL) {
                fprintf(stderr, "Your DISPLAY environment variable is not set.\n");
                fprintf(stderr, "Are you running i3-dump-log via SSH or on a virtual console?\n");
                fprintf(stderr, "Try DISPLAY=:0 i3-dump-log\n");
                exit(1);
            }
            fprintf(stderr, "FYI: The DISPLAY environment variable is set to \"%s\".\n", getenv("DISPLAY"));
            exit(1);
        }
        if (root_atom_contents("I3_CONFIG_PATH", conn, screen) != NULL) {
            fprintf(stderr, "i3-dump-log: ERROR: i3 is running, but SHM logging is not enabled.\n\n");
            if (!is_debug_build()) {
                fprintf(stderr, "You seem to be using a release version of i3:\n  %s\n\n", I3_VERSION);
                fprintf(stderr, "Release versions do not use SHM logging by default,\ntherefore i3-dump-log does not work.\n\n");
                fprintf(stderr, "Please follow this guide instead:\nhttp://i3wm.org/docs/debugging-release-version.html\n");
                exit(1);
            }
        }
        errx(EXIT_FAILURE, "Cannot get I3_SHMLOG_PATH atom contents. Is i3 running on this display?");
    }

    if (*shmname == '\0')
        errx(EXIT_FAILURE, "Cannot dump log: SHM logging is disabled in i3.");

    struct stat statbuf;

    /* NB: While we must never write, we need O_RDWR for the pthread condvar. */
    int logbuffer_shm = shm_open(shmname, O_RDWR, 0);
    if (logbuffer_shm == -1)
        err(EXIT_FAILURE, "Could not shm_open SHM segment for the i3 log (%s)", shmname);

    if (fstat(logbuffer_shm, &statbuf) != 0)
        err(EXIT_FAILURE, "stat(%s)", shmname);

    /* NB: While we must never write, we need PROT_WRITE for the pthread condvar. */
    logbuffer = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, logbuffer_shm, 0);
    if (logbuffer == MAP_FAILED)
        err(EXIT_FAILURE, "Could not mmap SHM segment for the i3 log");

    header = (i3_shmlog_header *)logbuffer;

    if (verbose)
        printf("next_write = %d, last_wrap = %d, logbuffer_size = %d, shmname = %s\n",
               header->offset_next_write, header->offset_last_wrap, header->size, shmname);
    walk = logbuffer + header->offset_next_write;

    /* We first need to print old content in case there was at least one
     * wrapping already. */

    if (*walk != '\0') {
        /* In case there was a write to the buffer already, skip the first
         * old line, it very likely is mangled. Not a problem, though, the log
         * is chatty enough to have plenty lines left. */
        while (*walk != '\n')
            walk++;
        walk++;
    }

    /* In case there was no wrapping, this is a no-op, otherwise it prints the
     * old lines. */
    wrap_count = 0;
    check_for_wrap();

    /* Then start from the beginning and print the newer lines */
    walk = logbuffer + sizeof(i3_shmlog_header);
    print_till_end();

    if (follow) {
        /* Since pthread_cond_wait() expects a mutex, we need to provide one.
         * To not lock i3 (that’s bad, mhkay?) we just define one outside of
         * the shared memory. */
        pthread_mutex_t dummy_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&dummy_mutex);
        while (1) {
            pthread_cond_wait(&(header->condvar), &dummy_mutex);
            /* If this was not a spurious wakeup, print the new lines. */
            if (header->offset_next_write != offset_next_write) {
                offset_next_write = header->offset_next_write;
                print_till_end();
            }
        }
    }

    return 0;
}
