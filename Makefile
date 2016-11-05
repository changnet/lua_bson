##### Build defaults #####
CC = gcc -std=gnu99
TARGET_SO =         lua_bson.so
TARGET_A  =         liblua_bson.a
PREFIX =            /usr/local
CFLAGS =            -g3 -Wall -pedantic -fno-inline

# libbson link with pthread,you need to load pthread library before debug
# LD_PRELOAD=/lib/x86_64-linux-gnu/libpthread.so.0 gdb -args lua test.lua
#CFLAGS =            -O3 -Wall -pedantic -DNDEBUG
LUA_BSON_CFLAGS =      -fpic
LUA_BSON_LDFLAGS =     -shared

LUA_BSON_DEPS = -I$(PREFIX)/include $(shell pkg-config --cflags --libs libbson-1.0)

AR= ar rcu
RANLIB= ranlib

OBJS =              lbson.o

.PHONY: all clean test

.c.o:
	$(CC) -c $(CFLAGS) $(LUA_BSON_CFLAGS) -o $@ $< $(LUA_BSON_DEPS)

all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): $(OBJS)
	$(CC) $(LDFLAGS) $(LUA_BSON_LDFLAGS) -o $@ $(OBJS) $(LUA_BSON_DEPS)

$(TARGET_A): $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

test:
	gcc -o writer writer.c $(LUA_BSON_DEPS)
	./writer
	lua test.lua

clean:
	rm -f *.o $(TARGET_SO) $(TARGET_A)
