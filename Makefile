##### Build defaults #####
CC = gcc
TARGET_SO =         lua_bson.so
TARGET_A  =         liblua_bson.a
PREFIX =            /usr/local
# CFLAGS =            -g3 -std=gnu99 -Wall -pedantic -fno-inline

# libbson link with pthread,you need to load pthread library before debug
# LD_PRELOAD=/lib/x86_64-linux-gnu/libpthread.so.0 gdb -args lua test.lua
CFLAGS =            -O2 -std=gnu99 -Wall -pedantic #-DNDEBUG
LUA_BSON_CFLAGS =      -fpic
LUA_BSON_LDFLAGS =     -shared

LUA_BSON_DEPS = -I$(PREFIX)/include $(shell pkg-config --cflags --libs libbson-1.0)

AR= ar rcu
RANLIB= ranlib

SHAREDDIR = .sharedlib
STATICDIR = .staticlib

OBJS =              lbson.o

SHAREDOBJS = $(addprefix $(SHAREDDIR)/,$(OBJS))
STATICOBJS = $(addprefix $(STATICDIR)/,$(OBJS))

DEPS := $(SHAREDOBJS + STATICOBJS:.o=.d)

.PHONY: all clean test

$(SHAREDDIR)/%.o: %.c
	@[ ! -d $(SHAREDDIR) ] & mkdir -p $(SHAREDDIR)
	$(CC) -c $(CFLAGS) -fPIC -o $@ $< $(LUA_BSON_DEPS)

$(STATICDIR)/%.o: %.c
	@[ ! -d $(STATICDIR) ] & mkdir -p $(STATICDIR)
	$(CC) -c $(CFLAGS) -fPIC -o $@ $< $(LUA_BSON_DEPS)

all: $(TARGET_SO) $(TARGET_A)

$(TARGET_SO): $(SHAREDOBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LUA_BSON_DEPS)

$(TARGET_A): $(STATICOBJS)
	$(AR) $@ $^
	$(RANLIB) $@

test:
	gcc -o writer writer.c $(LUA_BSON_DEPS)
	./writer
	lua test.lua

clean:
	rm -f -R *.o ./writer $(TARGET_SO) $(TARGET_A) $(STATICDIR) $(SHAREDDIR)
