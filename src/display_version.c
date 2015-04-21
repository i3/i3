#undef I3__FILE__
#define I3__FILE__ "key_press.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * display_version.c: displays the running i3 version, runs as part of
 *                    i3 --moreversion.
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include "all.h"

static bool human_readable_key;
static char *human_readable_version;

static int version_string(void *ctx, const unsigned char *val, size_t len) {
    if (human_readable_key)
        sasprintf(&human_readable_version, "%.*s", (int)len, val);
    return 1;
}

static int version_map_key(void *ctx, const unsigned char *stringval, size_t stringlen) {
    human_readable_key = (stringlen == strlen("human_readable") &&
                          strncmp((const char *)stringval, "human_readable", strlen("human_readable")) == 0);
    return 1;
}

static yajl_callbacks version_callbacks = {
    .yajl_string = version_string,
    .yajl_map_key = version_map_key,
};

/*
 * Connects to i3 to find out the currently running version. Useful since it
 * might be different from the version compiled into this binary (maybe the
 * user didn’t correctly install i3 or forgot te restart it).
 *
 * The output looks like this:
 * Running i3 version: 4.2-202-gb8e782c (2012-08-12, branch "next") (pid 14804)
 *
 * The i3 binary you just called: /home/michael/i3/i3
 * The i3 binary you are running: /home/michael/i3/i3
 *
 */
void display_running_version(void) {
    char *socket_path = root_atom_contents("I3_SOCKET_PATH", conn, conn_screen);
    if (socket_path == NULL)
        exit(EXIT_SUCCESS);

    char *pid_from_atom = root_atom_contents("I3_PID", conn, conn_screen);
    if (pid_from_atom == NULL) {
        /* If I3_PID is not set, the running version is older than 4.2-200. */
        printf("\nRunning version: < 4.2-200\n");
        exit(EXIT_SUCCESS);
    }

    /* Inform the user of what we are doing. While a single IPC request is
     * really fast normally, in case i3 hangs, this will not terminate. */
    printf("(Getting version from running i3, press ctrl-c to abort…)");
    fflush(stdout);

    /* TODO: refactor this with the code for sending commands */
    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd == -1)
        err(EXIT_FAILURE, "Could not create socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0)
        err(EXIT_FAILURE, "Could not connect to i3");

    if (ipc_send_message(sockfd, 0, I3_IPC_MESSAGE_TYPE_GET_VERSION,
                         (uint8_t *)"") == -1)
        err(EXIT_FAILURE, "IPC: write()");

    uint32_t reply_length;
    uint32_t reply_type;
    uint8_t *reply;
    int ret;
    if ((ret = ipc_recv_message(sockfd, &reply_type, &reply_length, &reply)) != 0) {
        if (ret == -1)
            err(EXIT_FAILURE, "IPC: read()");
        exit(EXIT_FAILURE);
    }

    if (reply_type != I3_IPC_MESSAGE_TYPE_GET_VERSION)
        errx(EXIT_FAILURE, "Got reply type %d, but expected %d (GET_VERSION)", reply_type, I3_IPC_MESSAGE_TYPE_GET_VERSION);

    yajl_handle handle = yajl_alloc(&version_callbacks, NULL, NULL);

    yajl_status state = yajl_parse(handle, (const unsigned char *)reply, (int)reply_length);
    if (state != yajl_status_ok)
        errx(EXIT_FAILURE, "Could not parse my own reply. That's weird. reply is %.*s", (int)reply_length, reply);

    printf("\rRunning i3 version: %s (pid %s)\n", human_readable_version, pid_from_atom);

#ifdef __linux__
    size_t destpath_size = 1024;
    ssize_t linksize;
    char *exepath;
    char *destpath = smalloc(destpath_size);

    sasprintf(&exepath, "/proc/%d/exe", getpid());

    while ((linksize = readlink(exepath, destpath, destpath_size)) == (ssize_t)destpath_size) {
        destpath_size = destpath_size * 2;
        destpath = srealloc(destpath, destpath_size);
    }
    if (linksize == -1)
        err(EXIT_FAILURE, "readlink(%s)", exepath);

    /* readlink() does not NULL-terminate strings, so we have to. */
    destpath[linksize] = '\0';

    printf("\n");
    printf("The i3 binary you just called: %s\n", destpath);

    free(exepath);
    sasprintf(&exepath, "/proc/%s/exe", pid_from_atom);

    while ((linksize = readlink(exepath, destpath, destpath_size)) == (ssize_t)destpath_size) {
        destpath_size = destpath_size * 2;
        destpath = srealloc(destpath, destpath_size);
    }
    if (linksize == -1)
        err(EXIT_FAILURE, "readlink(%s)", exepath);

    /* readlink() does not NULL-terminate strings, so we have to. */
    destpath[linksize] = '\0';

    /* Check if "(deleted)" is the readlink result. If so, the running version
     * does not match the file on disk. */
    if (strstr(destpath, "(deleted)") != NULL)
        printf("RUNNING BINARY DIFFERENT FROM BINARY ON DISK!\n");

    /* Since readlink() might put a "(deleted)" somewhere in the buffer and
     * stripping that out seems hackish and ugly, we read the process’s argv[0]
     * instead. */
    free(exepath);
    sasprintf(&exepath, "/proc/%s/cmdline", pid_from_atom);

    int fd;
    if ((fd = open(exepath, O_RDONLY)) == -1)
        err(EXIT_FAILURE, "open(%s)", exepath);
    if (read(fd, destpath, sizeof(destpath)) == -1)
        err(EXIT_FAILURE, "read(%s)", exepath);
    close(fd);

    printf("The i3 binary you are running: %s\n", destpath);

    free(exepath);
    free(destpath);
#endif

    yajl_free(handle);
}
