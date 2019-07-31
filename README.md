# lua_bson
[![Build Status](https://travis-ci.org/changnet/lua_bson.svg?branch=master)](https://travis-ci.org/changnet/lua_bson)

a bson encode/decode lua c module base on libbson.See more about libbson at
https://github.com/mongodb/libbson

Installation
------------

 * Make sure lua(5.3.4) and libbson(1.14.0) develop environment already installed
 * git clone https://github.com/changnet/lua_bson.git
 * cd lua_bson
 * make
 * make test
 * Copy lua_bson.so to your lua project's c module directory

or embed to your project

Api
----

```lua

-- encode lua table into a bson buffer
buffer,error = encode( tbl,nothrow )

-- decode a bson buffer into a lua table
tbl,error = decode( buffer,nothrow )

-- generate a object id
objectid = object_id()

-- encode stack variable into a bson buffer
buffer,error = encode_stack( nothrow,... )

-- decode a bson buffer into stack
... = decode_stack( nothrow,buffer )
```

If success,error always be nil.It raise a error if nothrow is false when error 
occur.if nothrow is true,error is the error message and other return data is 
invalid.

Example
-------

See 'test.lua'. A bson binary buffer decode as a lua table,like:  

```lua
table: 0x6a1f80
{
    empty_array = table: 0x6a2280
    {
    }
    empty_object = table: 0x6a22f0
    {
    }
    sparse = table: 0x6a2380
    {
        10 = number ten
    }
    force_object = table: 0x6a2450
    {
        2 = UK
        3 = CH
        1 = USA
    }
    employees = table: 0x6a23c0
    {
        1 = table: 0x6a2400
        {
            firstName = Bill
            lastName = Gates
        }
        2 = table: 0x6a2730
        {
            firstName = George
            lastName = Bush
        }
        3 = table: 0x6a27c0
        {
            firstName = Thomas
            lastName = Carter
        }
    }
    force_array = table: 0x6a25b0
    {
        1 = 123456789
        2 = 987654321
    }
}
```

Note
----
> {"hello": "world","lua": "bson"} in binary format:  
>
> 24 00 00 00 02 68 65 6c 6c 6f 00 06 00 00 00 77  
> 6f 72 6c 64 00 02 6c 75 61 00 05 00 00 00 62 73  
> 6f 6e 00 00 00 00 00 00 00 00 00 00 00 00 00 00  
> 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  


| binary(hex)       | meaning                             |
| -------------     |:-------------:                      |
| 24 00 00 00       | total document size(4bytes)         |
| 02                | value type( 02 is String)           |
| 68 65 6c 6c 6f 00 | key("hello") end with 0x00          |
| 06 00 00 00       | value size(4bytes)                  |
| 77 6f 72 6c 64 00 | value("world") end with 0x00        |
| 02 ... 6f 6e 00   | "lua": "bson"                       |
| 00                | type EOO ('end of object')          |
| 00 ...            | padding,a bson doc at least 64bytes |

* root document always be document,can't be array.
* find a key need to iterate every key.

See more at http://bsonspec.org/faq.html
