/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-msg/main.c: Utility which sends messages to a running i3-instance using
 * IPC via UNIX domain sockets.
 *
 * This (in combination with libi3/ipc_send_message.c and
 * libi3/ipc_recv_message.c) serves as an example for how to send your own
 * messages to i3.
 *
 * Additionally, it's even useful sometimes :-).
 *
 */
#include "libi3.h"

#include <err.h>
#include <getopt.h>
#include <i3/ipc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <yyjson.h>

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

static int exit_code = 0;

/*
 * Parses the reply to a RUN_COMMAND message and prints errors if any.
 * Returns true on success, false on JSON parse error.
 */
static bool parse_reply(const char *reply, size_t reply_length) {
    yyjson_doc *doc = yyjson_read(reply, reply_length, 0);
    if (!doc) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);

    /* The reply is an array of results, one per command */
    if (yyjson_is_arr(root)) {
        size_t idx, max;
        yyjson_val *result;
        yyjson_arr_foreach(root, idx, max, result) {
            if (!yyjson_is_obj(result)) {
                continue;
            }

            yyjson_val *success_val = yyjson_obj_get(result, "success");
            bool success = success_val && yyjson_get_bool(success_val);

            if (!success) {
                yyjson_val *input_val = yyjson_obj_get(result, "input");
                yyjson_val *errorposition_val = yyjson_obj_get(result, "errorposition");
                yyjson_val *error_val = yyjson_obj_get(result, "error");

                if (input_val && yyjson_is_str(input_val)) {
                    fprintf(stderr, "ERROR: Your command: %s\n", yyjson_get_str(input_val));
                }
                if (errorposition_val && yyjson_is_str(errorposition_val)) {
                    fprintf(stderr, "ERROR:               %s\n", yyjson_get_str(errorposition_val));
                }
                if (error_val && yyjson_is_str(error_val)) {
                    fprintf(stderr, "ERROR: %s\n", yyjson_get_str(error_val));
                }
                exit_code = 2;
            }
        }
    }

    yyjson_doc_free(doc);
    return true;
}

/*
 * Parses a GET_CONFIG reply and prints the config to stdout.
 * Returns true on success, false on JSON parse error.
 */
static bool parse_config_reply(const char *reply, size_t reply_length) {
    yyjson_doc *doc = yyjson_read(reply, reply_length, 0);
    if (!doc) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);

    if (yyjson_is_obj(root)) {
        yyjson_val *config_val = yyjson_obj_get(root, "config");
        if (config_val && yyjson_is_str(config_val)) {
            fprintf(stdout, "%s", yyjson_get_str(config_val));
        }
    }

    yyjson_doc_free(doc);
    return true;
}

int main(int argc, char *argv[]) {
    char *socket_path = NULL;
    int o, option_index = 0;
    uint32_t message_type = I3_IPC_MESSAGE_TYPE_RUN_COMMAND;
    char *payload = NULL;
    bool quiet = false;
    bool monitor = false;
    bool raw_reply = false;

    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"type", required_argument, 0, 't'},
        {"version", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"monitor", no_argument, 0, 'm'},
        {"help", no_argument, 0, 'h'},
        {"raw", no_argument, 0, 'r'},
        {0, 0, 0, 0}};

    char *options_string = "s:t:vhqmr";

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
            } else if (strcasecmp(optarg, "get_binding_state") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_BINDING_STATE;
            } else if (strcasecmp(optarg, "get_version") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_VERSION;
            } else if (strcasecmp(optarg, "get_config") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_GET_CONFIG;
            } else if (strcasecmp(optarg, "send_tick") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_SEND_TICK;
            } else if (strcasecmp(optarg, "subscribe") == 0) {
                message_type = I3_IPC_MESSAGE_TYPE_SUBSCRIBE;
            } else {
                printf("Unknown message type\n");
                printf("Known types: run_command, get_workspaces, get_outputs, get_tree, get_marks, get_bar_config, get_binding_modes, get_binding_state, get_version, get_config, send_tick, subscribe\n");
                exit(EXIT_FAILURE);
            }
        } else if (o == 'q') {
            quiet = true;
        } else if (o == 'm') {
            monitor = true;
        } else if (o == 'v') {
            printf("i3-msg " I3_VERSION "\n");
            return 0;
        } else if (o == 'h') {
            printf("i3-msg " I3_VERSION "\n");
            printf("i3-msg [-s <socket>] [-t <type>] [-m] <message>\n");
            return 0;
        } else if (o == '?') {
            exit(EXIT_FAILURE);
        } else if (o == 'r') {
            raw_reply = true;
        }
    }

    if (monitor && message_type != I3_IPC_MESSAGE_TYPE_SUBSCRIBE) {
        fprintf(stderr, "The monitor option -m is used with -t SUBSCRIBE exclusively.\n");
        exit(EXIT_FAILURE);
    }

    /* Use all arguments, separated by whitespace, as payload.
     * This way, you don't have to do i3-msg 'mark foo', you can use
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

    if (!payload) {
        payload = sstrdup("");
    }

    int sockfd = ipc_connect(socket_path);
    if (ipc_send_message(sockfd, strlen(payload), message_type, (uint8_t *)payload) == -1) {
        err(EXIT_FAILURE, "IPC: write()");
    }
    free(payload);

    uint32_t reply_length;
    uint32_t reply_type;
    uint8_t *reply;
    int ret;
    if ((ret = ipc_recv_message(sockfd, &reply_type, &reply_length, &reply)) != 0) {
        if (ret == -1) {
            err(EXIT_FAILURE, "IPC: read()");
        }
        exit(1);
    }
    if (reply_type != message_type) {
        errx(EXIT_FAILURE, "IPC: Received reply of type %d but expected %d", reply_type, message_type);
    }
    /* For the reply of commands, have a look if that command was successful.
     * If not, nicely format the error message. */
    if (reply_type == I3_IPC_REPLY_TYPE_COMMAND) {
        if (!raw_reply) {
            if (!parse_reply((const char *)reply, reply_length)) {
                errx(EXIT_FAILURE, "IPC: Could not parse JSON reply.");
            }
        }

        if (!quiet || raw_reply) {
            printf("%.*s\n", reply_length, reply);
        }
    } else if (reply_type == I3_IPC_REPLY_TYPE_CONFIG) {
        if (raw_reply) {
            printf("%.*s\n", reply_length, reply);
        } else {
            if (!parse_config_reply((const char *)reply, reply_length)) {
                errx(EXIT_FAILURE, "IPC: Could not parse JSON reply.");
            }
        }
    } else if (reply_type == I3_IPC_REPLY_TYPE_SUBSCRIBE) {
        do {
            free(reply);
            if ((ret = ipc_recv_message(sockfd, &reply_type, &reply_length, &reply)) != 0) {
                if (ret == -1) {
                    err(EXIT_FAILURE, "IPC: read()");
                }
                exit(1);
            }

            if (!(reply_type & I3_IPC_EVENT_MASK)) {
                errx(EXIT_FAILURE, "IPC: Received reply of type %d but expected an event", reply_type);
            }

            if (!quiet) {
                fprintf(stdout, "%.*s\n", reply_length, reply);
                fflush(stdout);
            }
        } while (monitor);
    } else {
        if (!quiet) {
            printf("%.*s\n", reply_length, reply);
        }
    }

    free(reply);

    close(sockfd);

    return exit_code;
}
