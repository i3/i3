/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * log.c: Logging functions.
 *
 */
#pragma once

#include <stdarg.h>
#include <stdbool.h>

/* We will include libi3.h which define its own version of LOG, ELOG.
 * We want *our* version, so we undef the libi3 one. */
#if defined(LOG)
#undef LOG
#endif
#if defined(ELOG)
#undef ELOG
#endif
#if defined(DLOG)
#undef DLOG
#endif
/** ##__VA_ARGS__ means: leave out __VA_ARGS__ completely if it is empty, that
   is, delete the preceding comma */
#define LOG(fmt, ...) verboselog(fmt, ##__VA_ARGS__)
#define ELOG(fmt, ...) errorlog("ERROR: " fmt, ##__VA_ARGS__)
#define DLOG(fmt, ...) debuglog("%s:%s:%d - " fmt, I3__FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)

extern char *errorfilename;
extern char *shmlogname;
extern int shmlog_size;

/**
 * Initializes logging by creating an error logfile in /tmp (or
 * XDG_RUNTIME_DIR, see get_process_filename()).
 *
 */
void init_logging(void);

/**
 * Opens the logbuffer.
 *
 */
void open_logbuffer(void);

/**
 * Closes the logbuffer.
 *
 */
void close_logbuffer(void);

/**
 * Checks if debug logging is active.
 *
 */
bool get_debug_logging(void);

/**
 * Set debug logging.
 *
 */
void set_debug_logging(const bool _debug_logging);

/**
 * Set verbosity of i3. If verbose is set to true, informative messages will
 * be printed to stdout. If verbose is set to false, only errors will be
 * printed.
 *
 */
void set_verbosity(bool _verbose);

/**
 * Logs the given message to stdout while prefixing the current time to it,
 * but only if debug logging was activated.
 *
 */
void debuglog(char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * Logs the given message to stdout while prefixing the current time to it.
 *
 */
void errorlog(char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * Logs the given message to stdout while prefixing the current time to it,
 * but only if verbose mode is activated.
 *
 */
void verboselog(char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * Deletes the unused log files. Useful if i3 exits immediately, eg.
 * because --get-socketpath was called. We don't care for syscall
 * failures. This function is invoked automatically when exiting.
 */
void purge_zerobyte_logfile(void);
