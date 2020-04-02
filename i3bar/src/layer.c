/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * layer.c: Handle layer events and show current layer in the bar
 *
 */
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

/* A datatype to pass through the callbacks to save the state */
struct layer_json_params {
    char *json;
    char *cur_key;
    layer *layer;
};

/*
 * Parse a string (change)
 *
 */
static int layer_string_cb(void *params_, const unsigned char *val, size_t len) {
    struct layer_json_params *params = (struct layer_json_params *)params_;

    if (!strcmp(params->cur_key, "name")) {
      char *temp_str;
      sasprintf(&temp_str, "%.*s", len, val);
      params->layer->name = i3string_from_utf8(temp_str);
      FREE(temp_str);
      
      FREE(params->cur_key);
      return 1;
    }

    FREE(params->cur_key);
    return 0;
}


/*
 * Parse a key.
 *
 * Essentially we just save it in the parsing state
 *
 */
static int layer_map_key_cb(void *params_, const unsigned char *keyVal, size_t keyLen) {
    struct layer_json_params *params = (struct layer_json_params *)params_;
    FREE(params->cur_key);
    sasprintf(&(params->cur_key), "%.*s", keyLen, keyVal);
    return 1;
}

static int layer_end_map_cb(void *params_) {
    struct layer_json_params *params = (struct layer_json_params *)params_;

    // calculate the predicted text width
    params->layer->name_width = predict_text_width(params->layer->name);
    FREE(params->cur_key);

    return 1;
}

static int layer_integer_cb(void *params_, long long val) {
  struct layer_json_params *params = (struct layer_json_params *)params_;

  if (!strcmp(params->cur_key, "from")) {
    params->layer->from = val;
    return 1;
  } else if (!strcmp(params->cur_key, "to")) {
    params->layer->to = val;
    return 1;
  }
    
  return 0;
}

/* A datastructure to pass all these callbacks to yajl */
static yajl_callbacks mode_callbacks = {
    .yajl_string = layer_string_cb,
    .yajl_integer = layer_integer_cb,
    .yajl_map_key = layer_map_key_cb,
    .yajl_end_map = layer_end_map_cb,
};


/*
 * Start parsing the received JSON string
 *
 */
void parse_layer_json(char *json) {
    struct layer_json_params params;

    layer new_layer;

    params.cur_key = NULL;
    params.json = json;
    params.layer = &new_layer;

    yajl_handle handle;
    yajl_status state;

    handle = yajl_alloc(&mode_callbacks, NULL, (void *)&params);

    state = yajl_parse(handle, (const unsigned char *)json, strlen(json));

    /* FIXME: Proper error handling for JSON parsing */
    switch (state) {
        case yajl_status_ok:
            break;
        case yajl_status_client_canceled:
        case yajl_status_error:
            ELOG("Could not parse layer event!\n");
            exit(EXIT_FAILURE);
            break;
    }

    /* We don't want to indicate default layer */
    if (strcmp("default", i3string_as_utf8(params.layer->name)) == 0)
        I3STRING_FREE(params.layer->name);

    /* Set the new layer */
    set_current_layer(&new_layer);

    yajl_free(handle);

    FREE(params.cur_key);
}
