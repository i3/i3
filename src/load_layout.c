#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>

#include "all.h"

/* TODO: refactor the whole parsing thing */

static char *last_key;
static Con *json_node;
static bool parsing_swallows;
static bool parsing_rect;
struct Match *current_swallow;

static int json_start_map(void *ctx) {
    LOG("start of map\n");
    if (parsing_swallows) {
        LOG("TODO: create new swallow\n");
        current_swallow = scalloc(sizeof(Match));
        TAILQ_INSERT_TAIL(&(json_node->swallow_head), current_swallow, matches);
    } else {
        if (!parsing_rect)
            json_node = con_new(json_node);
    }
    return 1;
}

static int json_end_map(void *ctx) {
    LOG("end of map\n");
    if (!parsing_swallows && !parsing_rect)
        json_node = json_node->parent;
    if (parsing_rect)
        parsing_rect = false;
    return 1;
}

static int json_end_array(void *ctx) {
    LOG("end of array\n");
    parsing_swallows = false;
    return 1;
}

static int json_key(void *ctx, const unsigned char *val, unsigned int len) {
    LOG("key: %.*s\n", len, val);
    FREE(last_key);
    last_key = scalloc((len+1) * sizeof(char));
    memcpy(last_key, val, len);
    if (strcasecmp(last_key, "swallows") == 0) {
        parsing_swallows = true;
    }
    if (strcasecmp(last_key, "rect") == 0)
        parsing_rect = true;
    return 1;
}

static int json_string(void *ctx, const unsigned char *val, unsigned int len) {
    LOG("string: %.*s for key %s\n", len, val, last_key);
    if (parsing_swallows) {
        /* TODO: the other swallowing keys */
        if (strcasecmp(last_key, "class") == 0) {
            current_swallow->class = scalloc((len+1) * sizeof(char));
            memcpy(current_swallow->class, val, len);
        }
        LOG("unhandled yet: swallow\n");
    } else {
        if (strcasecmp(last_key, "name") == 0) {
            json_node->name = scalloc((len+1) * sizeof(char));
            memcpy(json_node->name, val, len);
        }
    }
    return 1;
}

static int json_int(void *ctx, long val) {
    LOG("int %d for key %s\n", val, last_key);
    if (strcasecmp(last_key, "orientation") == 0) {
        json_node->orientation = val;
    }
    if (strcasecmp(last_key, "layout") == 0) {
        json_node->layout = val;
    }
    if (strcasecmp(last_key, "type") == 0) {
        json_node->type = val;
    }
    if (strcasecmp(last_key, "fullscreen_mode") == 0) {
        json_node->fullscreen_mode = val;
    }

    if (parsing_rect) {
        if (strcasecmp(last_key, "x") == 0)
            json_node->rect.x = val;
        else if (strcasecmp(last_key, "y") == 0)
            json_node->rect.y = val;
        else if (strcasecmp(last_key, "width") == 0)
            json_node->rect.width = val;
        else if (strcasecmp(last_key, "height") == 0)
            json_node->rect.height = val;
        else printf("WARNING: unknown key %s in rect\n", last_key);
        printf("rect now: (%d, %d, %d, %d)\n",
                json_node->rect.x, json_node->rect.y,
                json_node->rect.width, json_node->rect.height);
    }
    if (parsing_swallows) {
        if (strcasecmp(last_key, "id") == 0) {
            current_swallow->id = val;
        }
    }

    return 1;
}

static int json_double(void *ctx, double val) {
    LOG("double %f for key %s\n", val, last_key);
    if (strcasecmp(last_key, "percent") == 0) {
        json_node->percent = val;
    }
    return 1;
}

void tree_append_json(const char *filename) {
    /* TODO: percent of other windows are not correctly fixed at the moment */
    FILE *f;
    if ((f = fopen(filename, "r")) == NULL) {
        LOG("Cannot open file\n");
        return;
    }
    char *buf = malloc(65535); /* TODO */
    int n = fread(buf, 1, 65535, f);
    LOG("read %d bytes\n", n);
    yajl_gen g;
    yajl_handle hand;
    yajl_callbacks callbacks;
    memset(&callbacks, '\0', sizeof(yajl_callbacks));
    callbacks.yajl_start_map = json_start_map;
    callbacks.yajl_end_map = json_end_map;
    callbacks.yajl_end_array = json_end_array;
    callbacks.yajl_string = json_string;
    callbacks.yajl_map_key = json_key;
    callbacks.yajl_integer = json_int;
    callbacks.yajl_double = json_double;
    g = yajl_gen_alloc(NULL, NULL);
    hand = yajl_alloc(&callbacks, NULL, NULL, (void*)g);
    yajl_status stat;
    json_node = focused;
    setlocale(LC_NUMERIC, "C");
    stat = yajl_parse(hand, (const unsigned char*)buf, n);
    if (stat != yajl_status_ok &&
        stat != yajl_status_insufficient_data)
    {
        unsigned char * str = yajl_get_error(hand, 1, (const unsigned char*)buf, n);
        fprintf(stderr, (const char *) str);
        yajl_free_error(hand, str);
    }

    setlocale(LC_NUMERIC, "");
    yajl_parse_complete(hand);

    fclose(f);
    //con_focus(json_node);
}
