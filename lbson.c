#include "lbson.h"

#include <stdio.h> /* for snprintf */
#include <math.h>  /* for floor */
#include <assert.h>

#define MAX_LUA_STACK   1024
#define MAX_KEY_LENGTH  64
#define MAX_ARRAY_INDEX INT_MAX
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

int bson_decode( lua_State*L,bson_iter_t *iter,
    bson_type_t root_type,struct error_collector *ec );

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
                /* int32 or int64 */
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

int value_decode( lua_State*L,bson_iter_t *iter,struct error_collector *ec )
{
    switch ( bson_iter_type( iter ) )
    {
        case BSON_TYPE_DOUBLE    :
        {
            double val = bson_iter_double( iter );
            lua_pushnumber( L,val );
        }break;
        case BSON_TYPE_DOCUMENT  :
        {
            bson_iter_t sub_iter;
            if ( !bson_iter_recurse( iter, &sub_iter ) )
            {
                ERROR( ec,"bson document iter recurse error" );
                return -1;
            }
            if ( bson_decode( L,&sub_iter,BSON_TYPE_DOCUMENT,ec ) < 0 )
            {
                return -1;
            }
        }break;
        case BSON_TYPE_ARRAY     :
        {
            bson_iter_t sub_iter;
            if ( !bson_iter_recurse( iter, &sub_iter ) )
            {
                ERROR( ec,"bson array iter recurse error" );
                return -1;
            }
            if ( bson_decode( L,&sub_iter,BSON_TYPE_ARRAY,ec ) < 0 )
            {
                return -1;
            }
        }break;
        case BSON_TYPE_BINARY    :
        {
            const char *val  = NULL;
            unsigned int len = 0;
            bson_iter_binary( iter,NULL,&len,(const uint8_t **)(&val) );
            lua_pushlstring( L,val,len );
        }break;
        case BSON_TYPE_UTF8      :
        {
            unsigned int len = 0;
            const char *val = bson_iter_utf8( iter,&len );
            lua_pushlstring( L,val,len );
        }break;
        case BSON_TYPE_OID       :
        {
            const bson_oid_t *oid = bson_iter_oid ( iter );

            char str[25];  /* bson api make it 25 */
            bson_oid_to_string( oid, str );
            lua_pushstring( L,str );
        }break;
        case BSON_TYPE_BOOL      :
        {
            bool val = bson_iter_bool( iter );
            lua_pushboolean( L,val );
        }break;
        case BSON_TYPE_NULL      :
        {
            /* NULL == nil in lua */
            lua_pushnil( L );
        }break;
        case BSON_TYPE_INT32     :
        {
            int val = bson_iter_int32( iter );
            lua_pushinteger( L,val );
        }break;
        case BSON_TYPE_DATE_TIME :
        {
            /* A 64-bit integer containing the number of milliseconds since
             * the UNIX epoch
             */
            int64_t val = bson_iter_date_time( iter );
            lua_pushinteger( L,val );
        }break;
        case BSON_TYPE_INT64     :
        {
            int64_t val = bson_iter_int64( iter );
            lua_pushinteger( L,val );
        }break;
        default :
        {
            ERROR( ec,"unknow bson type:%d",bson_iter_type( iter ) );
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
                snprintf( key,MAX_KEY_LENGTH,"%d",cur_index++ );
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
                        snprintf( key,MAX_KEY_LENGTH,
                            LUA_INTEGER_FMT,lua_tointeger( L,-2 ) );
                    else
                        snprintf( key,MAX_KEY_LENGTH,
                            LUA_NUMBER_FMT,lua_tonumber( L,-2 ) );
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


/* decode a bson document into a lua table,push the table into stack
 * https://docs.mongodb.org/v3.0/reference/bson-types/
 * {
 * BSON_TYPE_EOD           = 0x00,
 * BSON_TYPE_DOUBLE        = 0x01,
 * BSON_TYPE_UTF8          = 0x02,
 * BSON_TYPE_DOCUMENT      = 0x03,
 * BSON_TYPE_ARRAY         = 0x04,
 * BSON_TYPE_BINARY        = 0x05,
 * BSON_TYPE_UNDEFINED     = 0x06,
 * BSON_TYPE_OID           = 0x07,
 * BSON_TYPE_BOOL          = 0x08,
 * BSON_TYPE_DATE_TIME     = 0x09,
 * BSON_TYPE_NULL          = 0x0A,
 * BSON_TYPE_REGEX         = 0x0B,
 * BSON_TYPE_DBPOINTER     = 0x0C,
 * BSON_TYPE_CODE          = 0x0D,
 * BSON_TYPE_SYMBOL        = 0x0E,
 * BSON_TYPE_CODEWSCOPE    = 0x0F,
 * BSON_TYPE_INT32         = 0x10,
 * BSON_TYPE_TIMESTAMP     = 0x11,
 * BSON_TYPE_INT64         = 0x12,
 * BSON_TYPE_MAXKEY        = 0x7F,
 * BSON_TYPE_MINKEY        = 0xFF,
 * } bson_type_t;
*/
int bson_decode( lua_State*L,bson_iter_t *iter,
    bson_type_t root_type,struct error_collector *ec )
{
    if ( lua_gettop(L) > MAX_LUA_STACK || !lua_checkstack(L,3) )
    {
        ERROR( ec,"bson_decode stack overflow" );
        return -1;
    }

    lua_newtable( L );
    while ( bson_iter_next( iter ) )
    {
        const char *key = bson_iter_key( iter );
        if ( value_decode( L,iter,ec ) < 0 )
        {
            lua_pop( L,1 );
            return      -1;
        }

        if ( BSON_TYPE_ARRAY == root_type )
        {
            /* lua array index start from 1
             * lua_rawseti will set nil value in a sparse array
             */
            lua_seti( L,-2,strtol(key,NULL,10) + 1 );
        }
        else
        {
            /* no lua_rawsetfield ?? */
            lua_setfield( L,-2,key );
        }
    }

    return 0;
}

int lbs_do_decode( lua_State *L,
    const bson_t *doc,bson_type_t root_type,struct error_collector *ec )
{
    bson_iter_t iter;
    if ( !bson_iter_init( &iter, doc ) )
    {
        ERROR( ec,"invalid bson document" );

       return -1;
    }

    return bson_decode( L,&iter,root_type,ec );
}

/* encode varibale in lua stack start from index
 * only number、table、boolean support.other type
 * will raise a error
 */
bson_t *lbs_do_encode_stack( lua_State *L,
    int index,struct error_collector *ec )
{
    int top = lua_gettop( L );
    if ( index > top )
    {
        ERROR( ec,"nothing in stack to be encoded" );

        return NULL;
    }

    bson_t *doc = bson_new();
    char key[MAX_KEY_LENGTH] = { 0 };
    for ( int i = index;i <= top;i ++ )
    {
        snprintf( key,MAX_KEY_LENGTH,"%d",i - index );
        if ( value_encode( L,doc,key,i,ec ) < 0 )
        {
            bson_destroy( doc );
            return         NULL;
        }
    }

    return doc;
}

/* decode doc into lua stack
 * return the number of variable push into stack
 */
int lbs_do_decode_stack( lua_State *L,
    const bson_t *doc,struct error_collector *ec )
{
    bson_iter_t iter;
    if ( !bson_iter_init( &iter, doc ) )
    {
        ERROR( ec,"invalid bson document" );

       return -1;
    }

    int cnt = 0;
    int top = lua_gettop( L );
    while ( bson_iter_next( &iter ) )
    {
        if ( cnt + top > MAX_LUA_STACK || !lua_checkstack( L,1 ) )
        {
            lua_settop( L,top );
            ERROR( ec,"lbs_decode_stack stack overflow" );
            return -1;
        }

        if ( value_decode( L,&iter,ec ) < 0 )
        {
            lua_settop( L,top );
            return      -1;
        }

        ++cnt;
    }

    return cnt;
}

/* encode lua table into a bson buffer */
static int lbs_encode( lua_State *L )
{
    struct error_collector ec;
    ec.what[0] = 0;

    int success = 0;
    int nothrow = lua_toboolean( L,2 );

    if ( !lua_istable( L,1 ) )
    {
        snprintf( ec.what,LBS_MAX_ERROR_MSG,"argument #1 table expected,got %s",
                    lua_typename( L,lua_type(L,1) ) );
        lua_pushnil( L ); /* fail,make sure buffer is nil */
    }
    else
    {
        bson_t *doc = lbs_do_encode( L,1,NULL,&ec );
        if ( doc )
        {
            const char *buffer = (const char *)bson_get_data( doc );
            lua_pushlstring( L,buffer,doc->len );

            bson_destroy( doc );
            success = 1;
            return    1;
        }

        lua_pushnil( L ); /* fail,make sure buffer is nil */
    }

    if ( !success )
    {
        if ( nothrow ) lua_pushstring( L,ec.what );
        else luaL_error( L,ec.what );
    }

    return 2;
}

/* decode a bson buffer into a lua table */
static int lbs_decode( lua_State *L )
{
    struct error_collector ec;
    ec.what[0] = 0;

    int nothrow = lua_toboolean( L,2 );
    if ( !lua_isstring( L,1 ) )
    {
        snprintf( ec.what,LBS_MAX_ERROR_MSG,"argument #1 string expected,got %s",
                    lua_typename( L,lua_type(L,1) ) );
        goto ERROR;
    }

    size_t sz = 0;
    const char *buffer = luaL_tolstring( L,1,&sz );

    bson_reader_t *reader = bson_reader_new_from_data( (const uint8_t *)buffer,sz );

    const bson_t *doc = bson_reader_read( reader,NULL );

    if ( !doc )
    {
        ERROR( (&ec),"invalid bson buffer" );

        goto ERROR;
    }

    /* root type always be a document in bson */
    if ( lbs_do_decode( L,doc,BSON_TYPE_DOCUMENT,&ec ) >= 0 )
    {
        bson_reader_destroy( reader );
        return 1;
    }

ERROR:
    bson_reader_destroy( reader );
    if ( !nothrow )
    {
        luaL_error( L,ec.what );
        return 0; /* in fact,it never return */
    }

    /* do not raise a error,return error message instead */
    lua_pushnil( L ); /* fail,make sure buffer is nil */
    lua_pushstring( L,ec.what );

    return 2;
}

/* generate a objectid */
static int lbs_object_id( lua_State *L )
{
    bson_oid_t oid;

    bson_oid_init( &oid, NULL );

    const int sz = 25;
    char str[sz];

    bson_oid_to_string( &oid, str );

    lua_pushlstring( L,str,sz );

    return 1;
}

static int lbs_encode_stack( lua_State *L )
{
    struct error_collector ec;
    ec.what[0] = 0;

    int nothrow = lua_toboolean( L,1 );
    bson_t *doc = lbs_do_encode_stack( L,2,&ec );
    if ( doc )
    {
        const char *buffer = (const char *)bson_get_data( doc );
        lua_pushlstring( L,buffer,doc->len );

        bson_destroy( doc );

        return 1;
    }

    if ( !nothrow )
    {
        luaL_error( L,ec.what );
        return 0; /* in fact,it never return */
    }

    /* do not raise a error,return error message instead */
    lua_pushnil( L ); /* fail,make sure buffer is nil */
    lua_pushstring( L,ec.what );

    return 2;
}

static int lbs_decode_stack( lua_State *L )
{
    struct error_collector ec;
    ec.what[0] = 0;

    int nothrow = lua_toboolean( L,2 );

    size_t sz = 0;
    const char *buffer = luaL_tolstring( L,1,&sz );

    bson_reader_t *reader = bson_reader_new_from_data( (const uint8_t *)buffer,sz );

    const bson_t *doc = bson_reader_read( reader,NULL );
    if ( !doc )
    {
        ERROR( (&ec),"invalid bson buffer" );

        goto ERROR;
    }

    int num = lbs_do_decode_stack( L,doc,&ec );
    if ( num > 0 )
    {
        bson_reader_destroy( reader );
        return num;
    }

ERROR:
    bson_reader_destroy( reader );
    if ( !nothrow )
    {
        luaL_error( L,ec.what );
        return 0; /* in fact,it never return */
    }

    /* do not raise a error,return error message instead */
    lua_pushnil( L ); /* fail,make sure buffer is nil */
    lua_pushstring( L,ec.what );

    return 2;
}

/* ====================LIBRARY INITIALISATION FUNCTION======================= */

static const luaL_Reg lua_parson_lib[] =
{
    {"encode", lbs_encode},
    {"decode", lbs_decode},
    {"object_id",lbs_object_id},
    {"encode_stack",lbs_encode_stack},
    {"decode_stack",lbs_decode_stack},
    {NULL, NULL}
};

int luaopen_lua_bson(lua_State *L)
{
    luaL_checkversion( L );
    luaL_newlib(L, lua_parson_lib);
    return 1;
}
