/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2015 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-msg/reformat.c: Reformat the json response from the IPC.
 *
 */

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#define GEN_AND_RETURN(func)                                          \
  {                                                                   \
    yajl_gen_status __stat = func;                                    \
    return __stat == yajl_gen_status_ok; }

static int reformat_null(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_null(g));
}
static int reformat_boolean(void * ctx, int boolean)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_bool(g, boolean));
}
static int reformat_number(void * ctx, const char * s, size_t l)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_number(g, s, l));
}
static int reformat_string(void * ctx, const unsigned char * stringVal,
                           size_t stringLen)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_string(g, stringVal, stringLen));
}
static int reformat_map_key(void * ctx, const unsigned char * stringVal,
                            size_t stringLen)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_string(g, stringVal, stringLen));
}
static int reformat_start_map(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_map_open(g));
}
static int reformat_end_map(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_map_close(g));
}
static int reformat_start_array(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_array_open(g));
}
static int reformat_end_array(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    GEN_AND_RETURN(yajl_gen_array_close(g));
}
static yajl_callbacks callbacks = {
    reformat_null,
    reformat_boolean,
    NULL,
    NULL,
    reformat_number,
    reformat_string,
    reformat_start_map,
    reformat_map_key,
    reformat_end_map,
    reformat_start_array,
    reformat_end_array
};

yajl_status beautify_json(yajl_gen g, const unsigned char *data, int len) {
    yajl_gen_config(g, yajl_gen_beautify, 1);
    yajl_handle hand = yajl_alloc(&callbacks, NULL, (void *) g);
    yajl_status stat = yajl_parse(hand, data, len);
    yajl_free(hand);
    return stat;
}
