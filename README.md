# lua_bson
a bson encode/decode lua c module

Api
----

```lua

-- encode lua table into a bson buffer
buffer,error = encode( tbl,nothrow )

-- decode a bson buffer into a lua table
tbl,error = decode( buffer,nothrow )
```

if success,error always be nil.

Note
----

http://bsonspec.org/faq.html

>
> {"hello": "world"} ==>> \x16\x00\x00\x00\x02


| binary(hex)   | meaning       |
| ------------- |:-------------:|
| \x16\x00\x00\x00      | right-aligned |
| col 2 is      | centered      |
| zebra stripes | are neat      |
>
>  \x16\x00\x00\x00                   // total document size  
>  \x02                               // 0x02 = type String  
>  hello\x00                          // field name  
>  \x06\x00\x00\x00world\x00          // field value  
>  \x00                               // 0x00 = type EOO ('end of object')  
