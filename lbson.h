#ifndef __LBSON_H
#define __LBSON_H

#define LBS_MAX_ERROR_MSG   256

/* collect error info */
struct error_collector
{
    char what[LBS_MAX_ERROR_MSG];
};

#ifdef __cplusplus
extern "C"
{
#endif

#include <bson.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

bson_t *lbs_do_encode( lua_State *L,
    int index,int *array,struct error_collector *ec );

int lbs_do_decode( lua_State *L,
    const bson_t *doc,struct error_collector *ec );

extern int luaopen_lua_bson( lua_State *L );

#ifdef __cplusplus
}
#endif

#endif /* __LBSON_H */
