/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-msg/main.c: Utility which sends messages to a running i3-instance using
 * IPC via UNIX domain sockets.
 *
 * This (in combination with libi3/ipc_send_message.c and
 * libi3/ipc_recv_message.c) serves as an example for how to send your own
 * messages to i3.
 *
 * Additionally, it’s even useful sometimes :-).
 *
 */
#include "libi3.h"

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

#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include <i3/ipc.h>

/*
 * Having verboselog() and errorlog() is necessary when using libi3.
 *
 */
void verboselog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

void errorlog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static char *last_key = NULL;

typedef struct reply_t {
    bool success;
    char *error;
    char *input;
    char *errorposition;
} reply_t;

static reply_t last_reply;

static int reply_boolean_cb(void *params, int val) {
    if (strcmp(last_key, "success") == 0)
        last_reply.success = val;
    return 1;
}

static int reply_string_cb(void *params, const unsigned char *val, size_t len) {
    char *str = scalloc(len + 1, 1);
    strncpy(str, (const char *)val, len);
    if (strcmp(last_key, "error") == 0)
        last_reply.error = str;
    else if (strcmp(last_key, "input") == 0)
        last_reply.input = str;
    else if (strcmp(last_key, "errorposition") == 0)
        last_reply.errorposition = str;
    else
        free(str);
    return 1;
}

static int reply_start_map_cb(void *params) {
    return 1;
}

static int reply_end_map_cb(void *params) {
    if (!last_reply.success) {
        fprintf(stderr, "ERROR: Your command: %s\n", last_reply.input);
        fprintf(stderr, "ERROR:               %s\n", last_reply.errorposition);
        fprintf(stderr, "ERROR: %s\n", last_reply.error);
    }
    return 1;
}

static int reply_map_key_cb(void *params, const unsigned char *keyVal, size_t keyLen) {
    free(last_key);
    last_key = scalloc(keyLen + 1, 1);
    strncpy(last_key, (const char *)keyVal, keyLen);
    return 1;
}

static yajl_callbacks reply_callbacks = {
    .yajl_boolean = reply_boolean_cb,
    .yajl_string = reply_string_cb,
    .yajl_start_map = reply_start_map_cb,
    .yajl_map_key = reply_map_key_cb,
    .yajl_end_map = reply_end_map_cb,
};

/*******************************************************************************
 * Config reply callbacks
 *******************************************************************************/

static char *config_last_key = NULL;

static int config_string_cb(void *params, const unsigned char *val, size_t len) {
    char *str = scalloc(len + 1, 1);
    strncpy(str, (const char *)val, len);
    if (strcmp(config_last_key, "config") == 0) {
        fprintf(stdout, "%s", str);
    }
    free(str);
    return 1;
}

static int config_start_map_cb(void *params) {
    return 1;
}

static int config_end_map_cb(void *params) {
    return 1;
}

static int config_map_key_cb(void *params, const unsigned char *keyVal, size_t keyLen) {
    config_last_key = scalloc(keyLen + 1, 1);
    strncpy(config_last_key, (const char *)keyVal, keyLen);
    return 1;
}

static yajl_callbacks config_callbacks = {
    .yajl_string = config_string_cb,
    .yajl_start_map = config_start_map_cb,
    .yajl_map_key = config_map_key_cb,
    .yajl_end_map = config_end_map_cb,
};

int main(int argc, char *argv[]) {
#if defined(__OpenBSD__)
    if (pledge("stdio rpath unix", NULL) == -1)
        err(EXIT_FAILURE, "pledge");
#endif
    char *socket_path = NULL;
    int o, option_index = 0;
    uint32_t message_type = I3_IPC_MESSAGE_TYPE_RUN_COMMAND;
    char *payload = NULL;
    bool quiet = false;

    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"type", required_argument, 0, 't'},
        {"version", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    char *options_string = "s:t:vhq";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        if (o == 's') {
            free(socket_path);
            socket_path = sstrdup(optarg);
        } else if (o == 't') {
            if (strcasecmp(optarg, "command") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_RUN_COMMAND;
            } else if (strcasecmp(optarg, "run_command") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_RUN_COMMAND;
            } else if (strcasecmp(optarg, "get_workspaces") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_WORKSPACES;
            } else if (strcasecmp(optarg, "get_outputs") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_OUTPUTS;
            } else if (strcasecmp(optarg, "get_tree") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_TREE;
            } else if (strcasecmp(optarg, "get_marks") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_MARKS;
            } else if (strcasecmp(optarg, "get_bar_config") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_BAR_CONFIG;
            } else if (strcasecmp(optarg, "get_binding_modes") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_BINDING_MODES;
            } else if (strcasecmp(optarg, "get_version") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_VERSION;
            } else if (strcasecmp(optarg, "get_config") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_CONFIG;
            } else if (strcasecmp(optarg, "send_tick") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_SEND_TICK;
            } else {
                printf("Unknown message type\n");
                printf("Known types: run_command, get_workspaces, get_outputs, get_tree, get_marks, get_bar_config, get_binding_modes, get_version, get_config, send_tick\n");
                exit(EXIT_FAILURE);
            }
        } else if (o == 'q') {
            quiet = true;
        } else if (o == 'v') {
            printf("i3-msg " I3_VERSION "\n");
            return 0;
        } else if (o == 'h') {
            printf("i3-msg " I3_VERSION "\n");
            printf("i3-msg [-s <socket>] [-t <type>] <message>\n");
            return 0;
        } else if (o == '?') {
            exit(EXIT_FAILURE);
        }
    }

    /* Use all arguments, separated by whitespace, as payload.
     * This way, you don’t have to do i3-msg 'mark foo', you can use
     * i3-msg mark foo */
    while (optind < argc) {
        if (!payload) {
            payload = sstrdup(argv[optind]);
        } else {
            char *both;
            sasprintf(&both, "%s %s", payload, argv[optind]);
            free(payload);
            payload = both;
        }
        optind++;
    }

    if (!payload)
        payload = sstrdup("");

    int sockfd = ipc_connect(socket_path);
    if (ipc_send_message(sockfd, strlen(payload), message_type, (uint8_t *)payload) == -1)
        err(EXIT_FAILURE, "IPC: write()");
    free(payload);

    if (quiet)
        return 0;

    uint32_t reply_length;
    uint32_t reply_type;
    uint8_t *reply;
    int ret;
    if ((ret = ipc_recv_message(sockfd, &reply_type, &reply_length, &reply)) != 0) {
        if (ret == -1)
            err(EXIT_FAILURE, "IPC: read()");
        exit(1);
    }
    if (reply_type != message_type)
        errx(EXIT_FAILURE, "IPC: Received reply of type %d but expected %d", reply_type, message_type);
    /* For the reply of commands, have a look if that command was successful.
     * If not, nicely format the error message. */
    if (reply_type == I3_IPC_REPLY_TYPE_COMMAND) {
        yajl_handle handle = yajl_alloc(&reply_callbacks, NULL, NULL);
        yajl_status state = yajl_parse(handle, (const unsigned char *)reply, reply_length);
        yajl_free(handle);

        switch (state) {
            case yajl_status_ok:
                break;
            case yajl_status_client_canceled:
            case yajl_status_error:
                errx(EXIT_FAILURE, "IPC: Could not parse JSON reply.");
        }

        /* NB: We still fall-through and print the reply, because even if one
         * command failed, that doesn’t mean that all commands failed. */
    } else if (reply_type == I3_IPC_REPLY_TYPE_CONFIG) {
        yajl_handle handle = yajl_alloc(&config_callbacks, NULL, NULL);
        yajl_status state = yajl_parse(handle, (const unsigned char *)reply, reply_length);
        yajl_free(handle);

        switch (state) {
            case yajl_status_ok:
                break;
            case yajl_status_client_canceled:
            case yajl_status_error:
                errx(EXIT_FAILURE, "IPC: Could not parse JSON reply.");
        }

        goto exit;
    }
    printf("%.*s\n", reply_length, reply);

exit:
    free(reply);

    close(sockfd);

    return 0;
}
