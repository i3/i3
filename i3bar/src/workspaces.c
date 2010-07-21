#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <yajl/yajl_parse.h>

#include "common.h"
#include "workspaces.h"
#include "ipc.h"

struct workspaces_json_params {
	i3_ws*	workspaces;
	i3_ws*	workspaces_walk;
	char*	cur_key;
	char*	json;
};

static int workspaces_null_cb(void* params_) {
	struct workspaces_json_params* params = (struct workspaces_json_params*) params_;

	if (strcmp(params->cur_key, "current_workspace")) {
		return 0;
	}

	FREE(params->cur_key);

	return 1;
}

static int workspaces_boolean_cb(void* params_, bool val) {
	struct workspaces_json_params* params = (struct workspaces_json_params*) params_;

	if (!strcmp(params->cur_key, "visible")) {
		params->workspaces_walk->visible = val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "focused")) {
		params->workspaces_walk->focused = val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "urgent")) {
		params->workspaces_walk->focused = val;
		FREE(params->cur_key);
		return 1;
	}

	FREE(params->cur_key);

	return 0;
}

static int workspaces_integer_cb(void* params_, long val) {
	struct workspaces_json_params* params = (struct workspaces_json_params*) params_;

	if (!strcmp(params->cur_key, "num")) {
		params->workspaces_walk->num = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "x")) {
		params->workspaces_walk->rect.x = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "y")) {
		params->workspaces_walk->rect.y = (int) val;
		FREE(params->cur_key);
		return 1;
	}
	
	if (!strcmp(params->cur_key, "width")) {
		params->workspaces_walk->rect.w = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "height")) {
		params->workspaces_walk->rect.h = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	FREE(params->cur_key);
	return 0;
}

static int workspaces_string_cb(void* params_, const unsigned char* val, unsigned int len) {
        struct workspaces_json_params* params = (struct workspaces_json_params*) params_;

        char* output_name;

        if (!strcmp(params->cur_key, "name")) {
                params->workspaces_walk->name = malloc(sizeof(const unsigned char) * (len + 1));
                strncpy(params->workspaces_walk->name, (const char*) val, len);
		params->workspaces_walk->name[len] = '\0';

                FREE(params->cur_key);

                return 1;
        }

        if (!strcmp(params->cur_key, "output")) {
                output_name = malloc(sizeof(const unsigned char) * (len + 1));
                strncpy(output_name, (const char*) val, len);
		output_name[len] = '\0';
                params->workspaces_walk->output = get_output_by_name(output_name);
                free(output_name);

                return 1;
        }

        return 0;
}

static int workspaces_start_map_cb(void* params_) {
	struct workspaces_json_params* params = (struct workspaces_json_params*) params_;
	i3_ws* new_workspace = NULL;

	if (params->cur_key == NULL) {
		new_workspace = malloc(sizeof(i3_ws));
		new_workspace->num = -1;
		new_workspace->name = NULL;
		new_workspace->visible = 0;
		new_workspace->focused = 0;
		new_workspace->urgent = 0;
		memset(&new_workspace->rect, 0, sizeof(rect));
		new_workspace->output = NULL;
		new_workspace->next = NULL;

		if (params->workspaces == NULL) {
			params->workspaces = new_workspace;
		} else {
			params->workspaces_walk->next = new_workspace;
		}

		params->workspaces_walk = new_workspace;
		return 1;
	}

	return 1;
}

static int workspaces_map_key_cb(void* params_, const unsigned char* keyVal, unsigned int keyLen) {
	struct workspaces_json_params* params = (struct workspaces_json_params*) params_;
	FREE(params->cur_key);

	params->cur_key = malloc(sizeof(unsigned char) * (keyLen + 1));
	strncpy(params->cur_key, (const char*) keyVal, keyLen);
	params->cur_key[keyLen] = '\0';

	return 1;
}

yajl_callbacks workspaces_callbacks = {
	&workspaces_null_cb,
	&workspaces_boolean_cb,
	&workspaces_integer_cb,
	NULL,
	NULL,
	&workspaces_string_cb,
	&workspaces_start_map_cb,
	&workspaces_map_key_cb,
	NULL,
	NULL,
	NULL
};

void got_workspaces_json_cb(char* json, void* params_) {
	/* FIXME: Fasciliate stream-processing, i.e. allow starting to interpret
	 * JSON in chunks */
	struct workspaces_json_params* params = (struct workspaces_json_params*) params_;

	yajl_handle handle;
	yajl_parser_config parse_conf = { 0, 0 };
	yajl_status state;
	
	params->json = json;

	handle = yajl_alloc(&workspaces_callbacks, &parse_conf, NULL, (void*) params);

	state = yajl_parse(handle, (const unsigned char*) json, strlen(json));

	/* FIXME: Propper errorhandling for JSON-parsing */
	switch (state) {
		case yajl_status_ok:
			break;
		case yajl_status_client_canceled:
		case yajl_status_insufficient_data:
		case yajl_status_error:
			printf("ERROR: Could not parse workspaces-reply!\n");
			exit(EXIT_FAILURE);
			break;
	}

	yajl_free(handle);

	free_workspaces();
	workspaces = params->workspaces;
	
	FREE(params->json);
	FREE(params);
}

void refresh_workspaces() {
	struct workspaces_json_params* params = malloc(sizeof(struct workspaces_json_params));

	params->workspaces = NULL;
	params->workspaces_walk = NULL;
	params->cur_key = NULL;
	params->json = NULL;

	get_workspaces_json(&got_workspaces_json_cb, params);
}

void free_workspaces() {
	i3_ws* tmp;
	while (workspaces != NULL) {
		tmp = workspaces;
		workspaces = workspaces->next;
		FREE(tmp->name);
		FREE(tmp);
	}
}
