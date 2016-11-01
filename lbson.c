#include "lbson.h"

#include <stdio.h> /* for snprintf */
#include <math.h>  /* for floor */
#include <assert.h>

#define MAX_LUA_STACK   1024
#define MAX_KEY_LENGTH  64
#define MAX_ARRAY_INDEX 10240
#define ARRAY_KEY       "__array"

#define ERROR(ector,...)    \
    do{snprintf( ector->what,LBS_MAX_ERROR_MSG,__VA_ARGS__ );}while(0)


/* check if a lua table is a array
 * 1.all key is integer
 * 2.metafield __array is true
 */
static int is_array( lua_State *L,int index,int *array,int *max_index )
{
    double key = 0;
    assert( array && max_index );

    /* set default value */
    *array = 0;
    *max_index = -1;

    if ( luaL_getmetafield( L,index,ARRAY_KEY ) )
    {
        if ( LUA_TNIL != lua_type( L,-1 ) )
        {
            if ( lua_toboolean( L,-1 ) )
                *array = 1;
            else
            {
                lua_pop( L,1 );
                return 0;  /* it's object,use default value */
            }
        }

        lua_pop( L,1 );    /* pop metafield value */
    }

    /* get max array index */
    lua_pushnil( L );
    while ( lua_next( L,index ) != 0 )
    {
        /* stack status:table, key, value */
        /* array index must be integer and >= 1 */
        if ( lua_type( L, -2 ) != LUA_TNUMBER )
        {
            *max_index = -1;
            lua_pop( L,2 ); /* pop both key and value */
            return 0;
        }

        /* key is not integer,treat as object */
        key = lua_tonumber( L, -2 );
        if ( floor(key) != key || key < 1 )
        {
            *max_index = -1;
            lua_pop( L,2 );
            return 0;
        }

        /* array index over MAX_ARRAY_INDEX,must be object */
        if ( key > MAX_ARRAY_INDEX )
        {
            *array = 0;
            *max_index = -1;
            lua_pop( L,2 );

            return 0;
        }

        if ( key > *max_index ) *max_index = (int)key;

        lua_pop( L, 1 );
    }

    if ( *max_index > 0 ) *array = 1;

    return 0;
}

/* check a integer is int32 or int64 */
static inline int lua_isbit32(int64_t v)
{
    return v >= INT_MIN && v <= INT_MAX;
}

int value_encode( lua_State *L,bson_t *doc,
    const char *key,int index,struct error_collector *ec )
{
    int ty = lua_type( L,index );
    switch ( ty )
    {
        case LUA_TNIL :
        {
            BSON_APPEND_NULL( doc,key );
        }break;
        case LUA_TBOOLEAN :
        {
            int val = lua_toboolean( L,index );
            BSON_APPEND_BOOL( doc,key,val );
        }break;
        case LUA_TNUMBER :
        {
            if ( lua_isinteger( L,index ) )
            {
                /* 有可能是int，也可能是int64 */
                int64_t val = lua_tointeger( L,index );
                if ( lua_isbit32( val ) )
                    BSON_APPEND_INT32( doc,key,val );
                else
                    BSON_APPEND_INT64( doc,key,val );
            }
            else
            {
                double val = lua_tonumber( L,index );
                BSON_APPEND_DOUBLE( doc,key,val );
            }
        }break;
        case LUA_TSTRING :
        {
            const char *val = lua_tostring( L,index );
            BSON_APPEND_UTF8( doc,key,val );
        }break;
        case LUA_TTABLE :
        {
            int array = 0;
            bson_t *sub_doc = lbs_do_encode( L,index,&array,ec );
            if ( !sub_doc ) return -1;

            if ( array ) BSON_APPEND_ARRAY( doc,key,sub_doc );
            else BSON_APPEND_DOCUMENT( doc,key,sub_doc );
            bson_destroy( sub_doc );
        }break;
        default :
        {
            ERROR( ec,"value_encode can not convert %s to bson value\n",
                lua_typename(L,ty) );
            return -1;
        }break;
    }

    return 0;
}

bson_t *lbs_do_encode( lua_State *L,
    int index,int *array,struct error_collector *ec )
{
    /* need 2 pos to iterate lua table */
    if ( lua_gettop( L ) > MAX_LUA_STACK || !lua_checkstack( L,2 ) )
    {
        ERROR( ec,"stack overflow" );
        return NULL;
    }

    int max_index  = -1;
    int _is_array  =  0;
    is_array( L,index,&_is_array,&max_index );

    int stack_top = lua_gettop( L );
    bson_t *doc = bson_new();

    if ( _is_array )   /* encode array */
    {
        if ( max_index > 0 ) /* a sparse array like { [10] = "foo" } */
        {
            char key[MAX_KEY_LENGTH] = { 0 };
            int cur_index = 0;
            /* lua array start from 1 */
            for ( cur_index = 0;cur_index < max_index;cur_index ++ )
            {
                snprintf( key,MAX_KEY_LENGTH,"%d",cur_index );
                lua_rawgeti( L, index, cur_index + 1 );
                if ( value_encode( L,doc,key,stack_top + 1,ec ) < 0 )
                {
                    lua_pop( L,1 );
                    bson_destroy( doc );

                    return NULL;
                }

                lua_pop( L,1 );
            }
        }
        else
        {
            /* force a table(with string key) as a array */
            int cur_index = 0;
            char key[MAX_KEY_LENGTH] = { 0 };

            lua_pushnil( L );
            while ( lua_next( L,index) != 0 )
            {
                snprintf( key,MAX_KEY_LENGTH,"%d",cur_index );
                if ( value_encode( L,doc,key,stack_top + 2,ec ) < 0 )
                {
                    lua_pop( L,2 );
                    bson_destroy( doc );

                    return NULL;
                }

                lua_pop( L,1 );
            }
        }
    }
    else   /* encode object */
    {
        lua_pushnil(L);  /* first key */
        while ( lua_next(L, index) != 0 )
        {
            char key[MAX_KEY_LENGTH] = { 0 };
            /* 'key' (at index -2) and 'value' (at index -1) */
            const char *pkey = NULL;
            switch ( lua_type( L,-2 ) )
            {
                case LUA_TBOOLEAN :
                {
                    if ( lua_toboolean( L,-2 ) )
                        snprintf( key,MAX_KEY_LENGTH,"true" );
                    else
                        snprintf( key,MAX_KEY_LENGTH,"false" );
                    pkey = key;
                }break;
                case LUA_TNUMBER  :
                {
                    if ( lua_isinteger( L,-2 ) )
                        snprintf( key,MAX_KEY_LENGTH,LUA_INTEGER_FMT,lua_tointeger( L,-2 ) );
                    else
                        snprintf( key,MAX_KEY_LENGTH,LUA_NUMBER_FMT,lua_tonumber( L,-2 ) );
                    pkey = key;
                }break;
                case LUA_TSTRING :
                {
                    size_t len = 0;
                    pkey = lua_tolstring( L,-2,&len );
                    if ( len > MAX_KEY_LENGTH - 1 )
                    {
                        bson_destroy( doc );
                        lua_pop( L,2 );
                        ERROR( ec,"lua table string key too long\n" );
                        return NULL;
                    }
                }break;
                default :
                {
                    bson_destroy( doc );
                    lua_pop( L,2 );
                    ERROR( ec,"can not convert %s to bson key\n",
                        lua_typename( L,lua_type( L,-2 ) ) );
                    return NULL;
                }break;
            }

            assert( pkey );
            if ( value_encode( L,doc,pkey,stack_top + 2,ec ) < 0 )
            {
                lua_pop( L,2 );
                bson_destroy( doc );

                return NULL;
            }

            /* removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }
    }

    assert( stack_top == lua_gettop(L) );

    if ( array ) *array = _is_array;

    return doc;
}

/* encode lua table into a bson buffer */
static int lbs_encode( lua_State *L )
{
    struct error_collector ec;
    ec.what[0] = 0;

    int success = false;
    int nothrow = lua_toboolean( L,2 );

    if ( !lua_istable( L,1 ) )
    {
        snprintf( ec.what,LBS_MAX_ERROR_MSG,"argument #1 table expected,got %s",
                    lua_typename( L,lua_type(L,1) ) );
        lua_pushnil( L );
    }
    else
    {
        const bson_t *doc = lbs_do_encode( L,1,NULL,&ec );
        if ( doc )
        {
            const char *buffer = (const char *)bson_get_data( doc );
            lua_pushlstring( L,buffer,doc->len );

            success = true;
            return 1;
        }
        else
        {
            lua_pushnil( L );
        }
    }

    if ( !success )
    {
        if ( nothrow ) lua_pushstring( L,ec.what );
        else luaL_error( L,ec.what );
    }

    return 2;
}

static int lbs_decode( lua_State *L )
{
    return 0;
}

/* ====================LIBRARY INITIALISATION FUNCTION======================= */

static const luaL_Reg lua_parson_lib[] =
{
    {"encode", lbs_encode},
    {"decode", lbs_decode},
    // {"encode_to_file", encode_to_file},
    // {"decode_from_file", decode_from_file},
    {NULL, NULL}
};

int luaopen_lua_bson(lua_State *L)
{
    luaL_newlib(L, lua_parson_lib);
    return 1;
}
