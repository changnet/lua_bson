##### Build defaults #####
CC = gcc -std=gnu99
TARGET_SO =         lua_bson.so
TARGET_A  =         liblua_bson.a
PREFIX =            /usr/local
CFLAGS =            -g3 -Wall -pedantic -fno-inline
#CFLAGS =            -O3 -Wall -pedantic -DNDEBUG
LUA_BSON_CFLAGS =      -fpic -I/usr/local/include/libbson-1.0
LUA_BSON_LDFLAGS =     -shared -lbson-1.0
LUA_INCLUDE_DIR =   $(PREFIX)/include
AR= ar rcu
RANLIB= ranlib

BUILD_CFLAGS =      -I$(LUA_INCLUDE_DIR) $(LUA_BSON_CFLAGS)
OBJS =              lbson.o

.PHONY: all clean test

.c.o:
	$(CC) -c $(CFLAGS) $(BUILD_CFLAGS) -o $@ $<

all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): $(OBJS)
	$(CC) $(LDFLAGS) $(LUA_BSON_LDFLAGS) -o $@ $(OBJS)

$(TARGET_A): $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

test:
	lua test.lua

clean:
	rm -f *.o $(TARGET_SO) $(TARGET_A)
