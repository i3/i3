#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <i3/ipc.h>

#include <yajl/yajl_parse.h>

#include "common.h"
#include "outputs.h"
#include "ipc.h"

struct outputs_json_params {
	struct outputs_head	*outputs;
	i3_output		*outputs_walk;
	char*			cur_key;
	char*			json;
};

static int outputs_null_cb(void* params_) {
	struct outputs_json_params* params = (struct outputs_json_params*) params_;

	if (strcmp(params->cur_key, "current_workspace")) {
		return 0;
	}

	FREE(params->cur_key);

	return 1;
}

static int outputs_boolean_cb(void* params_, bool val) {
	struct outputs_json_params* params = (struct outputs_json_params*) params_;

	if (strcmp(params->cur_key, "active")) {
		return 0;
	}

	params->outputs_walk->active = val;

	FREE(params->cur_key);

	return 1;
}

static int outputs_integer_cb(void* params_, long val) {
	struct outputs_json_params* params = (struct outputs_json_params*) params_;

	if (!strcmp(params->cur_key, "current_workspace")) {
		params->outputs_walk->ws = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "x")) {
		params->outputs_walk->rect.x = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "y")) {
		params->outputs_walk->rect.y = (int) val;
		FREE(params->cur_key);
		return 1;
	}
	
	if (!strcmp(params->cur_key, "width")) {
		params->outputs_walk->rect.w = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	if (!strcmp(params->cur_key, "height")) {
		params->outputs_walk->rect.h = (int) val;
		FREE(params->cur_key);
		return 1;
	}

	return 0;
}

static int outputs_string_cb(void* params_, const unsigned char* val, unsigned int len) {
        struct outputs_json_params* params = (struct outputs_json_params*) params_;
	
	if (strcmp(params->cur_key, "name")) {
		return 0;
	}

	params->outputs_walk->name = malloc(sizeof(const unsigned char) * (len + 1));
	strncpy(params->outputs_walk->name, (const char*) val, len);
	params->outputs_walk->name[len] = '\0';

	FREE(params->cur_key);
	
	return 1;
}

static int outputs_start_map_cb(void* params_) {
	struct outputs_json_params* params = (struct outputs_json_params*) params_;
	i3_output *new_output = NULL;

	if (params->cur_key == NULL) {
		new_output = malloc(sizeof(i3_output));
		new_output->name = NULL;
		new_output->ws = 0,
		memset(&new_output->rect, 0, sizeof(rect));
		new_output->bar = XCB_NONE;

		new_output->workspaces = malloc(sizeof(struct ws_head));
		TAILQ_INIT(new_output->workspaces);

		SLIST_INSERT_HEAD(params->outputs, new_output, slist);

		params->outputs_walk = SLIST_FIRST(params->outputs);

		return 1;
	}

	return 1;
}

static int outputs_map_key_cb(void* params_, const unsigned char* keyVal, unsigned int keyLen) {
	struct outputs_json_params* params = (struct outputs_json_params*) params_;
	FREE(params->cur_key);

	params->cur_key = malloc(sizeof(unsigned char) * (keyLen + 1));
	strncpy(params->cur_key, (const char*) keyVal, keyLen);
	params->cur_key[keyLen] = '\0';

	return 1;
}

yajl_callbacks outputs_callbacks = {
	&outputs_null_cb,
	&outputs_boolean_cb,
	&outputs_integer_cb,
	NULL,
	NULL,
	&outputs_string_cb,
	&outputs_start_map_cb,
	&outputs_map_key_cb,
	NULL,
	NULL,
	NULL
};

void parse_outputs_json(char* json) {
	/* FIXME: Fasciliate stream-processing, i.e. allow starting to interpret
	 * JSON in chunks */
	struct outputs_json_params params;
	printf(json);
	params.outputs = malloc(sizeof(struct outputs_head));
	SLIST_INIT(params.outputs);

	params.outputs_walk = NULL;
	params.cur_key = NULL;
	params.json = json;

	yajl_handle handle;
	yajl_parser_config parse_conf = { 0, 0 };
	yajl_status state;
	
	handle = yajl_alloc(&outputs_callbacks, &parse_conf, NULL, (void*) &params);

	state = yajl_parse(handle, (const unsigned char*) json, strlen(json));

	/* FIXME: Propper errorhandling for JSON-parsing */
	switch (state) {
		case yajl_status_ok:
			break;
		case yajl_status_client_canceled:
		case yajl_status_insufficient_data:
		case yajl_status_error:
			printf("ERROR: Could not parse outputs-reply!\n");
			exit(EXIT_FAILURE);
			break;
	}
	
	yajl_free(handle);

	if (outputs != NULL) {
		FREE_SLIST(outputs, i3_output);
	}

	outputs = params.outputs;
}

i3_output* get_output_by_name(char* name) {
	i3_output *walk;
	SLIST_FOREACH(walk, outputs, slist) {
		if (!strcmp(walk->name, name)) {
			break;
		}
	}

	return walk;
}
