#undef I3__FILE__
#define I3__FILE__ "log.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * log.c: Logging functions.
 *
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "util.h"
#include "log.h"
#include "i3.h"
#include "libi3.h"
#include "shmlog.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

static bool debug_logging = false;
static bool verbose = false;
static FILE *errorfile;
char *errorfilename;

/* SHM logging variables */

/* The name for the SHM (/i3-log-%pid). Will end up on /dev/shm on most
 * systems. Global so that we can clean up at exit. */
char *shmlogname = "";
/* Size limit for the SHM log, by default 25 MiB. Can be overwritten using the
 * flag --shmlog-size. */
int shmlog_size = 0;
/* If enabled, logbuffer will point to a memory mapping of the i3 SHM log. */
static char *logbuffer;
/* A pointer (within logbuffer) where data will be written to next. */
static char *logwalk;
/* A pointer to the shmlog header */
static i3_shmlog_header *header;
/* A pointer to the byte where we last wrapped. Necessary to not print the
 * left-overs at the end of the ringbuffer. */
static char *loglastwrap;
/* Size (in bytes) of the i3 SHM log. */
static int logbuffer_size;
/* File descriptor for shm_open. */
static int logbuffer_shm;

/*
 * Writes the offsets for the next write and for the last wrap to the
 * shmlog_header.
 * Necessary to print the i3 SHM log in the correct order.
 *
 */
static void store_log_markers(void) {
    header->offset_next_write = (logwalk - logbuffer);
    header->offset_last_wrap = (loglastwrap - logbuffer);
    header->size = logbuffer_size;
}

/*
 * Initializes logging by creating an error logfile in /tmp (or
 * XDG_RUNTIME_DIR, see get_process_filename()).
 *
 * Will be called twice if --shmlog-size is specified.
 *
 */
void init_logging(void) {
    if (!errorfilename) {
        if (!(errorfilename = get_process_filename("errorlog")))
            fprintf(stderr, "Could not initialize errorlog\n");
        else {
            errorfile = fopen(errorfilename, "w");
            if (fcntl(fileno(errorfile), F_SETFD, FD_CLOEXEC)) {
                fprintf(stderr, "Could not set close-on-exec flag\n");
            }
        }
    }
    /* Start SHM logging if shmlog_size is > 0. shmlog_size is SHMLOG_SIZE by
     * default on development versions, and 0 on release versions. If it is
     * not > 0, the user has turned it off, so let's close the logbuffer. */
    if (shmlog_size > 0 && logbuffer == NULL)
        open_logbuffer();
    else if (shmlog_size <= 0 && logbuffer)
        close_logbuffer();
    atexit(purge_zerobyte_logfile);
}

/*
 * Opens the logbuffer.
 *
 */
void open_logbuffer(void) {
    /* Reserve 1% of the RAM for the logfile, but at max 25 MiB.
         * For 512 MiB of RAM this will lead to a 5 MiB log buffer.
         * At the moment (2011-12-10), no testcase leads to an i3 log
         * of more than ~ 600 KiB. */
    long long physical_mem_bytes;
#if defined(__APPLE__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    size_t length = sizeof(long long);
    sysctl(mib, 2, &physical_mem_bytes, &length, NULL, 0);
#else
    physical_mem_bytes = (long long)sysconf(_SC_PHYS_PAGES) *
                         sysconf(_SC_PAGESIZE);
#endif
    logbuffer_size = min(physical_mem_bytes * 0.01, shmlog_size);
#if defined(__FreeBSD__)
    sasprintf(&shmlogname, "/tmp/i3-log-%d", getpid());
#else
    sasprintf(&shmlogname, "/i3-log-%d", getpid());
#endif
    logbuffer_shm = shm_open(shmlogname, O_RDWR | O_CREAT, S_IREAD | S_IWRITE);
    if (logbuffer_shm == -1) {
        fprintf(stderr, "Could not shm_open SHM segment for the i3 log: %s\n", strerror(errno));
        return;
    }

#if defined(__OpenBSD__) || defined(__APPLE__)
    if (ftruncate(logbuffer_shm, logbuffer_size) == -1) {
        fprintf(stderr, "Could not ftruncate SHM segment for the i3 log: %s\n", strerror(errno));
#else
    int ret;
    if ((ret = posix_fallocate(logbuffer_shm, 0, logbuffer_size)) != 0) {
        fprintf(stderr, "Could not ftruncate SHM segment for the i3 log: %s\n", strerror(ret));
#endif
        close(logbuffer_shm);
        shm_unlink(shmlogname);
        return;
    }

    logbuffer = mmap(NULL, logbuffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, logbuffer_shm, 0);
    if (logbuffer == MAP_FAILED) {
        close_logbuffer();
        fprintf(stderr, "Could not mmap SHM segment for the i3 log: %s\n", strerror(errno));
        return;
    }

    /* Initialize with 0-bytes, just to be sure… */
    memset(logbuffer, '\0', logbuffer_size);

    header = (i3_shmlog_header *)logbuffer;

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    if (pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED) != 0)
        fprintf(stderr, "pthread_condattr_setpshared() failed, i3-dump-log -f will not work!\n");
    pthread_cond_init(&(header->condvar), &cond_attr);

    logwalk = logbuffer + sizeof(i3_shmlog_header);
    loglastwrap = logbuffer + logbuffer_size;
    store_log_markers();
}

/*
 * Closes the logbuffer.
 *
 */
void close_logbuffer(void) {
    close(logbuffer_shm);
    shm_unlink(shmlogname);
    logbuffer = NULL;
    shmlogname = "";
}

/*
 * Set verbosity of i3. If verbose is set to true, informative messages will
 * be printed to stdout. If verbose is set to false, only errors will be
 * printed.
 *
 */
void set_verbosity(bool _verbose) {
    verbose = _verbose;
}

/*
 * Get debug logging.
 *
 */
bool get_debug_logging(void) {
    return debug_logging;
}

/*
 * Set debug logging.
 *
 */
void set_debug_logging(const bool _debug_logging) {
    debug_logging = _debug_logging;
}

/*
 * Logs the given message to stdout (if print is true) while prefixing the
 * current time to it. Additionally, the message will be saved in the i3 SHM
 * log if enabled.
 * This is to be called by *LOG() which includes filename/linenumber/function.
 *
 */
static void vlog(const bool print, const char *fmt, va_list args) {
    /* Precisely one page to not consume too much memory but to hold enough
     * data to be useful. */
    static char message[4096];
    static struct tm result;
    static time_t t;
    static struct tm *tmp;
    static size_t len;

    /* Get current time */
    t = time(NULL);
    /* Convert time to local time (determined by the locale) */
    tmp = localtime_r(&t, &result);
    /* Generate time prefix */
    len = strftime(message, sizeof(message), "%x %X - ", tmp);

    /*
     * logbuffer  print
     * ----------------
     *  true      true   format message, save, print
     *  true      false  format message, save
     *  false     true   print message only
     *  false     false  INVALID, never called
     */
    if (!logbuffer) {
#ifdef DEBUG_TIMING
        struct timeval tv;
        gettimeofday(&tv, NULL);
        printf("%s%d.%d - ", message, tv.tv_sec, tv.tv_usec);
#else
        printf("%s", message);
#endif
        vprintf(fmt, args);
    } else {
        len += vsnprintf(message + len, sizeof(message) - len, fmt, args);
        if (len >= sizeof(message)) {
            fprintf(stderr, "BUG: single log message > 4k\n");
        }

        /* If there is no space for the current message in the ringbuffer, we
         * need to wrap and write to the beginning again. */
        if (len >= (size_t)(logbuffer_size - (logwalk - logbuffer))) {
            loglastwrap = logwalk;
            logwalk = logbuffer + sizeof(i3_shmlog_header);
            store_log_markers();
            header->wrap_count++;
        }

        /* Copy the buffer, move the write pointer to the byte after our
         * current message. */
        strncpy(logwalk, message, len);
        logwalk += len;

        store_log_markers();

        /* Wake up all (i3-dump-log) processes waiting for condvar. */
        pthread_cond_broadcast(&(header->condvar));

        if (print)
            fwrite(message, len, 1, stdout);
    }
}

/*
 * Logs the given message to stdout while prefixing the current time to it,
 * but only if verbose mode is activated.
 *
 */
void verboselog(char *fmt, ...) {
    va_list args;

    if (!logbuffer && !verbose)
        return;

    va_start(args, fmt);
    vlog(verbose, fmt, args);
    va_end(args);
}

/*
 * Logs the given message to stdout while prefixing the current time to it.
 *
 */
void errorlog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vlog(true, fmt, args);
    va_end(args);

    /* also log to the error logfile, if opened */
    va_start(args, fmt);
    vfprintf(errorfile, fmt, args);
    fflush(errorfile);
    va_end(args);
}

/*
 * Logs the given message to stdout while prefixing the current time to it,
 * but only if debug logging was activated.
 * This is to be called by DLOG() which includes filename/linenumber
 *
 */
void debuglog(char *fmt, ...) {
    va_list args;

    if (!logbuffer && !(debug_logging))
        return;

    va_start(args, fmt);
    vlog(debug_logging, fmt, args);
    va_end(args);
}

/*
 * Deletes the unused log files. Useful if i3 exits immediately, eg.
 * because --get-socketpath was called. We don't care for syscall
 * failures. This function is invoked automatically when exiting.
 */
void purge_zerobyte_logfile(void) {
    struct stat st;
    char *slash;

    if (!errorfilename)
        return;

    /* don't delete the log file if it contains something */
    if ((stat(errorfilename, &st)) == -1 || st.st_size > 0)
        return;

    if (unlink(errorfilename) == -1)
        return;

    if ((slash = strrchr(errorfilename, '/')) != NULL) {
        *slash = '\0';
        /* possibly fails with ENOTEMPTY if there are files (or
         * sockets) left. */
        rmdir(errorfilename);
    }
}
