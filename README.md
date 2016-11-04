# lua_bson
a bson encode/decode lua c module

Api
----

```lua

-- encode lua table into a bson buffer
buffer,error = encode( tbl,nothrow )

-- decode a bson buffer into a lua table
tbl,error = decode( buffer,nothrow )

-- generate a objectid
objectid = object_id
```

if success,error always be nil.

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

* root document always be document
* find a key need to iterate every key

See more at http://bsonspec.org/faq.html
