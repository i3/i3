#undef I3__FILE__
#define I3__FILE__ "load_layout.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * load_layout.c: Restore (parts of) the layout, for example after an inplace
 *                restart.
 *
 */
#include "all.h"

#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

/* TODO: refactor the whole parsing thing */

static char *last_key;
static Con *json_node;
static Con *to_focus;
static bool parsing_swallows;
static bool parsing_rect;
static bool parsing_window_rect;
static bool parsing_geometry;
static bool parsing_focus;
struct Match *current_swallow;

/* This list is used for reordering the focus stack after parsing the 'focus'
 * array. */
struct focus_mapping {
    int old_id;
    TAILQ_ENTRY(focus_mapping) focus_mappings;
};

static TAILQ_HEAD(focus_mappings_head, focus_mapping) focus_mappings =
  TAILQ_HEAD_INITIALIZER(focus_mappings);

static int json_start_map(void *ctx) {
    LOG("start of map, last_key = %s\n", last_key);
    if (parsing_swallows) {
        LOG("creating new swallow\n");
        current_swallow = smalloc(sizeof(Match));
        match_init(current_swallow);
        TAILQ_INSERT_TAIL(&(json_node->swallow_head), current_swallow, matches);
    } else {
        if (!parsing_rect && !parsing_window_rect && !parsing_geometry) {
            if (last_key && strcasecmp(last_key, "floating_nodes") == 0) {
                DLOG("New floating_node\n");
                Con *ws = con_get_workspace(json_node);
                json_node = con_new_skeleton(NULL, NULL);
                json_node->parent = ws;
                DLOG("Parent is workspace = %p\n", ws);
            } else {
                Con *parent = json_node;
                json_node = con_new_skeleton(NULL, NULL);
                json_node->parent = parent;
            }
        }
    }
    return 1;
}

static int json_end_map(void *ctx) {
    LOG("end of map\n");
    if (!parsing_swallows && !parsing_rect && !parsing_window_rect && !parsing_geometry) {
        /* Set a few default values to simplify manually crafted layout files. */
        if (json_node->layout == L_DEFAULT) {
            DLOG("Setting layout = L_SPLITH\n");
            json_node->layout = L_SPLITH;
        }

        LOG("attaching\n");
        con_attach(json_node, json_node->parent, true);
        LOG("Creating window\n");
        x_con_init(json_node, json_node->depth);
        json_node = json_node->parent;
    }
    if (parsing_rect)
        parsing_rect = false;
    if (parsing_window_rect)
        parsing_window_rect = false;
    if (parsing_geometry)
        parsing_geometry = false;
    return 1;
}

static int json_end_array(void *ctx) {
    LOG("end of array\n");
    parsing_swallows = false;
    if (parsing_focus) {
        /* Clear the list of focus mappings */
        struct focus_mapping *mapping;
        TAILQ_FOREACH_REVERSE(mapping, &focus_mappings, focus_mappings_head, focus_mappings) {
            LOG("focus (reverse) %d\n", mapping->old_id);
            Con *con;
            TAILQ_FOREACH(con, &(json_node->focus_head), focused) {
                if (con->old_id != mapping->old_id)
                    continue;
                LOG("got it! %p\n", con);
                /* Move this entry to the top of the focus list. */
                TAILQ_REMOVE(&(json_node->focus_head), con, focused);
                TAILQ_INSERT_HEAD(&(json_node->focus_head), con, focused);
                break;
            }
        }
        while (!TAILQ_EMPTY(&focus_mappings)) {
            mapping = TAILQ_FIRST(&focus_mappings);
            TAILQ_REMOVE(&focus_mappings, mapping, focus_mappings);
            free(mapping);
        }
        parsing_focus = false;
    }
    return 1;
}

#if YAJL_MAJOR < 2
static int json_key(void *ctx, const unsigned char *val, unsigned int len) {
#else
static int json_key(void *ctx, const unsigned char *val, size_t len) {
#endif
    LOG("key: %.*s\n", (int)len, val);
    FREE(last_key);
    last_key = scalloc((len+1) * sizeof(char));
    memcpy(last_key, val, len);
    if (strcasecmp(last_key, "swallows") == 0)
        parsing_swallows = true;

    if (strcasecmp(last_key, "rect") == 0)
        parsing_rect = true;

    if (strcasecmp(last_key, "window_rect") == 0)
        parsing_window_rect = true;

    if (strcasecmp(last_key, "geometry") == 0)
        parsing_geometry = true;

    if (strcasecmp(last_key, "focus") == 0)
        parsing_focus = true;

    return 1;
}

#if YAJL_MAJOR >= 2
static int json_string(void *ctx, const unsigned char *val, size_t len) {
#else
static int json_string(void *ctx, const unsigned char *val, unsigned int len) {
#endif
    LOG("string: %.*s for key %s\n", (int)len, val, last_key);
    if (parsing_swallows) {
        /* TODO: the other swallowing keys */
        if (strcasecmp(last_key, "class") == 0) {
            char *sval;
            sasprintf(&sval, "%.*s", len, val);
            current_swallow->class = regex_new(sval);
            free(sval);
        } else {
            ELOG("swallow key %s unknown\n", last_key);
        }
    } else {
        if (strcasecmp(last_key, "name") == 0) {
            json_node->name = scalloc((len+1) * sizeof(char));
            memcpy(json_node->name, val, len);
        } else if (strcasecmp(last_key, "sticky_group") == 0) {
            json_node->sticky_group = scalloc((len+1) * sizeof(char));
            memcpy(json_node->sticky_group, val, len);
            LOG("sticky_group of this container is %s\n", json_node->sticky_group);
        } else if (strcasecmp(last_key, "orientation") == 0) {
            /* Upgrade path from older versions of i3 (doing an inplace restart
             * to a newer version):
             * "orientation" is dumped before "layout". Therefore, we store
             * whether the orientation was horizontal or vertical in the
             * last_split_layout. When we then encounter layout == "default",
             * we will use the last_split_layout as layout instead. */
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "none") == 0 ||
                strcasecmp(buf, "horizontal") == 0)
                json_node->last_split_layout = L_SPLITH;
            else if (strcasecmp(buf, "vertical") == 0)
                json_node->last_split_layout = L_SPLITV;
            else LOG("Unhandled orientation: %s\n", buf);
            free(buf);
        } else if (strcasecmp(last_key, "border") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "none") == 0)
                json_node->border_style = BS_NONE;
            else if (strcasecmp(buf, "1pixel") == 0) {
                json_node->border_style = BS_PIXEL;
                json_node->current_border_width = 1;
            } else if (strcasecmp(buf, "pixel") == 0)
                json_node->border_style = BS_PIXEL;
            else if (strcasecmp(buf, "normal") == 0)
                json_node->border_style = BS_NORMAL;
            else LOG("Unhandled \"border\": %s\n", buf);
            free(buf);
        } else if (strcasecmp(last_key, "layout") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "default") == 0)
                /* This set above when we read "orientation". */
                json_node->layout = json_node->last_split_layout;
            else if (strcasecmp(buf, "stacked") == 0)
                json_node->layout = L_STACKED;
            else if (strcasecmp(buf, "tabbed") == 0)
                json_node->layout = L_TABBED;
            else if (strcasecmp(buf, "dockarea") == 0)
                json_node->layout = L_DOCKAREA;
            else if (strcasecmp(buf, "output") == 0)
                json_node->layout = L_OUTPUT;
            else if (strcasecmp(buf, "splith") == 0)
                json_node->layout = L_SPLITH;
            else if (strcasecmp(buf, "splitv") == 0)
                json_node->layout = L_SPLITV;
            else LOG("Unhandled \"layout\": %s\n", buf);
            free(buf);
        } else if (strcasecmp(last_key, "workspace_layout") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "default") == 0)
                json_node->workspace_layout = L_DEFAULT;
            else if (strcasecmp(buf, "stacked") == 0)
                json_node->workspace_layout = L_STACKED;
            else if (strcasecmp(buf, "tabbed") == 0)
                json_node->workspace_layout = L_TABBED;
            else LOG("Unhandled \"workspace_layout\": %s\n", buf);
            free(buf);
        } else if (strcasecmp(last_key, "last_split_layout") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "splith") == 0)
                json_node->last_split_layout = L_SPLITH;
            else if (strcasecmp(buf, "splitv") == 0)
                json_node->last_split_layout = L_SPLITV;
            else LOG("Unhandled \"last_splitlayout\": %s\n", buf);
            free(buf);
        } else if (strcasecmp(last_key, "mark") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            json_node->mark = buf;
        } else if (strcasecmp(last_key, "floating") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "auto_off") == 0)
                json_node->floating = FLOATING_AUTO_OFF;
            else if (strcasecmp(buf, "auto_on") == 0)
                json_node->floating = FLOATING_AUTO_ON;
            else if (strcasecmp(buf, "user_off") == 0)
                json_node->floating = FLOATING_USER_OFF;
            else if (strcasecmp(buf, "user_on") == 0)
                json_node->floating = FLOATING_USER_ON;
            free(buf);
        } else if (strcasecmp(last_key, "scratchpad_state") == 0) {
            char *buf = NULL;
            sasprintf(&buf, "%.*s", (int)len, val);
            if (strcasecmp(buf, "none") == 0)
                json_node->scratchpad_state = SCRATCHPAD_NONE;
            else if (strcasecmp(buf, "fresh") == 0)
                json_node->scratchpad_state = SCRATCHPAD_FRESH;
            else if (strcasecmp(buf, "changed") == 0)
                json_node->scratchpad_state = SCRATCHPAD_CHANGED;
            free(buf);
        }
    }
    return 1;
}

#if YAJL_MAJOR >= 2
static int json_int(void *ctx, long long val) {
    LOG("int %lld for key %s\n", val, last_key);
#else
static int json_int(void *ctx, long val) {
    LOG("int %ld for key %s\n", val, last_key);
#endif
    if (strcasecmp(last_key, "type") == 0)
        json_node->type = val;

    if (strcasecmp(last_key, "fullscreen_mode") == 0)
        json_node->fullscreen_mode = val;

    if (strcasecmp(last_key, "num") == 0)
        json_node->num = val;

    if (strcasecmp(last_key, "current_border_width") == 0)
        json_node->current_border_width = val;

    if (strcasecmp(last_key, "depth") == 0)
        json_node->depth = val;

    if (!parsing_swallows && strcasecmp(last_key, "id") == 0)
        json_node->old_id = val;

    if (parsing_focus) {
        struct focus_mapping *focus_mapping = scalloc(sizeof(struct focus_mapping));
        focus_mapping->old_id = val;
        TAILQ_INSERT_TAIL(&focus_mappings, focus_mapping, focus_mappings);
    }

    if (parsing_rect || parsing_window_rect || parsing_geometry) {
        Rect *r;
        if (parsing_rect)
            r = &(json_node->rect);
        else if (parsing_window_rect)
            r = &(json_node->window_rect);
        else r = &(json_node->geometry);
        if (strcasecmp(last_key, "x") == 0)
            r->x = val;
        else if (strcasecmp(last_key, "y") == 0)
            r->y = val;
        else if (strcasecmp(last_key, "width") == 0)
            r->width = val;
        else if (strcasecmp(last_key, "height") == 0)
            r->height = val;
        else printf("WARNING: unknown key %s in rect\n", last_key);
        printf("rect now: (%d, %d, %d, %d)\n",
                r->x, r->y, r->width, r->height);
    }
    if (parsing_swallows) {
        if (strcasecmp(last_key, "id") == 0) {
            current_swallow->id = val;
        }
        if (strcasecmp(last_key, "dock") == 0) {
            current_swallow->dock = val;
        }
        if (strcasecmp(last_key, "insert_where") == 0) {
            current_swallow->insert_where = val;
        }
    }

    return 1;
}

static int json_bool(void *ctx, int val) {
    LOG("bool %d for key %s\n", val, last_key);
    if (strcasecmp(last_key, "focused") == 0 && val) {
        to_focus = json_node;
    }

    if (parsing_swallows) {
        if (strcasecmp(last_key, "restart_mode") == 0)
            current_swallow->restart_mode = val;
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
        LOG("Cannot open file \"%s\"\n", filename);
        return;
    }
    struct stat stbuf;
    if (fstat(fileno(f), &stbuf) != 0) {
        LOG("Cannot fstat() the file\n");
        fclose(f);
        return;
    }
    char *buf = smalloc(stbuf.st_size);
    int n = fread(buf, 1, stbuf.st_size, f);
    if (n != stbuf.st_size) {
        LOG("File \"%s\" could not be read entirely, not loading.\n", filename);
        fclose(f);
        return;
    }
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
    callbacks.yajl_boolean = json_bool;
#if YAJL_MAJOR >= 2
    g = yajl_gen_alloc(NULL);
    hand = yajl_alloc(&callbacks, NULL, (void*)g);
#else
    g = yajl_gen_alloc(NULL, NULL);
    hand = yajl_alloc(&callbacks, NULL, NULL, (void*)g);
#endif
    /* Allowing comments allows for more user-friendly layout files. */
    yajl_config(hand, yajl_allow_comments, true);
    yajl_status stat;
    json_node = focused;
    to_focus = NULL;
    parsing_rect = false;
    parsing_window_rect = false;
    parsing_geometry = false;
    setlocale(LC_NUMERIC, "C");
    stat = yajl_parse(hand, (const unsigned char*)buf, n);
    if (stat != yajl_status_ok)
    {
        unsigned char * str = yajl_get_error(hand, 1, (const unsigned char*)buf, n);
        fprintf(stderr, "%s\n", (const char *) str);
        yajl_free_error(hand, str);
    }

    setlocale(LC_NUMERIC, "");
#if YAJL_MAJOR >= 2
    yajl_complete_parse(hand);
#else
    yajl_parse_complete(hand);
#endif

    fclose(f);
    if (to_focus)
        con_focus(to_focus);
}
