/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
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

int main(int argc, char *argv[]) {
    int o, option_index = 0;
    bool verbose = false;

    static struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"verbose", no_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    char *options_string = "s:vVh";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        if (o == 'v') {
            printf("i3-dump-log " I3_VERSION "\n");
            return 0;
        } else if (o == 'V') {
            verbose = true;
        } else if (o == 'h') {
            printf("i3-dump-log " I3_VERSION "\n");
            printf("i3-dump-log [-s <socket>]\n");
            return 0;
        }
    }

    char *shmname = root_atom_contents("I3_SHMLOG_PATH");
    if (shmname == NULL)
        errx(EXIT_FAILURE, "Cannot get I3_SHMLOG_PATH atom contents. Is i3 running on this display?");

    if (*shmname == '\0')
        errx(EXIT_FAILURE, "Cannot dump log: SHM logging is disabled in i3.");

    struct stat statbuf;

    int logbuffer_shm = shm_open(shmname, O_RDONLY, 0);
    if (logbuffer_shm == -1)
        err(EXIT_FAILURE, "Could not shm_open SHM segment for the i3 log (%s)", shmname);

    if (fstat(logbuffer_shm, &statbuf) != 0)
        err(EXIT_FAILURE, "stat(%s)", shmname);

    char *logbuffer = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, logbuffer_shm, 0);
    if (logbuffer == MAP_FAILED)
        err(EXIT_FAILURE, "Could not mmap SHM segment for the i3 log");

    i3_shmlog_header *header = (i3_shmlog_header*)logbuffer;

    if (verbose)
        printf("next_write = %d, last_wrap = %d, logbuffer_size = %d, shmname = %s\n",
               header->offset_next_write, header->offset_last_wrap, header->size, shmname);
    int chars;
    char *walk = logbuffer + header->offset_next_write;
    /* Skip the first line, it very likely is mangled. Not a problem, though,
     * the log is chatty enough to have plenty lines left. */
    while (*walk != '\0')
        walk++;

    /* Print the oldest log lines. We use printf("%s") to stop on \0. */
    while (walk < (logbuffer + header->offset_last_wrap)) {
        chars = printf("%s", walk);
        /* Shortcut: If there are two consecutive \0 bytes, this part of the
         * buffer was never touched. To not call printf() for every byte of the
         * buffer, we directly exit the loop. */
        if (*walk == '\0' && *(walk+1) == '\0')
            break;
        walk += (chars > 0 ? chars : 1);
    }

    /* Then start from the beginning and print the newer lines */
    walk = logbuffer + sizeof(i3_shmlog_header);
    while (walk < (logbuffer + header->offset_next_write)) {
        chars = printf("%s", walk);
        walk += (chars > 0 ? chars : 1);
    }

    return 0;
}
