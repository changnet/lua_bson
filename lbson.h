#ifndef __LBSON_H
#define __LBSON_H

#define LBS_MAX_ERROR_MSG   256

/* bson-compat.h:28,bson-macros.h:31 had include C++ stl
 * so bson.h must outside of extern "C"
 */
#include <bson.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* collect error info */
struct error_collector
{
    char what[LBS_MAX_ERROR_MSG];
};

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* decode doc into lua stack
 * return the number of variable push into stack
 */
int lbs_decode_stack( lua_State *L,
    const bson_t *doc,struct error_collector *ec );

/* encode varibale in lua stack start from index
 * only number、table、boolean support.other type
 * will raise a error
 */
bson_t *lbs_encode_stack( lua_State *L,
    int index,struct error_collector *ec );

bson_t *lbs_do_encode( lua_State *L,
    int index,int *array,struct error_collector *ec );

int lbs_do_decode( lua_State *L,
    const bson_t *doc,bson_type_t root_type,struct error_collector *ec );

extern int luaopen_lua_bson( lua_State *L );

#ifdef __cplusplus
}
#endif

#endif /* __LBSON_H */
