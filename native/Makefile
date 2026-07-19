# JapanOSINT native backend — single build -> bin/japanosint
CC      ?= cc
REPO_ROOT := $(abspath ..)

PKG_CFLAGS := $(shell pkg-config --cflags libcurl openssl 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs   libcurl openssl 2>/dev/null)
# Homebrew OpenSSL fallback (macOS) if pkg-config didn't resolve it.
ifeq ($(strip $(PKG_LIBS)),)
  BREW_SSL := $(shell [ -d /opt/homebrew/opt/openssl@3 ] && echo /opt/homebrew/opt/openssl@3 || echo /opt/homebrew/opt/openssl)
  PKG_CFLAGS := -I$(BREW_SSL)/include
  PKG_LIBS   := -L$(BREW_SSL)/lib -lcurl -lssl -lcrypto
endif
# Prefer Homebrew curl (keg-only) when present: the Apple-shipped libcurl is
# built WITHOUT WebSocket support (lib/ws.c needs it). Homebrew curl is a
# strict superset for httpclient's GET/POST; rpath so it resolves at runtime.
BREW_CURL := $(shell [ -d /opt/homebrew/opt/curl ] && echo /opt/homebrew/opt/curl)
ifneq ($(strip $(BREW_CURL)),)
  PKG_CFLAGS := -I$(BREW_CURL)/include $(PKG_CFLAGS)
  PKG_LIBS   := -L$(BREW_CURL)/lib -Wl,-rpath,$(BREW_CURL)/lib $(PKG_LIBS)
endif

SQLITE_DEFS := -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_RTREE \
               -DSQLITE_ENABLE_JSON1 -DSQLITE_ENABLE_MATH_FUNCTIONS \
               -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_DEPRECATED

MECAB_CFLAGS := $(shell mecab-config --cflags 2>/dev/null)
MECAB_LIBS   := $(shell mecab-config --libs 2>/dev/null)

# iconv lives in a standalone libiconv on macOS/BSD but is folded into glibc on
# Linux, where there is no libiconv.so to link against. Only add -liconv off-Linux.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  ICONV_LIB :=
else
  ICONV_LIB := -liconv
endif

CFLAGS  ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -pthread
CFLAGS  += -DJO_REPO_ROOT='"$(REPO_ROOT)"' $(PKG_CFLAGS) $(MECAB_CFLAGS) -Ithird_party
# Auto-generate header dependency files (.d) next to each .o so editing a
# header (e.g. source.h, which defines the ABI-critical source_def struct)
# forces a recompile of every TU that includes it. Without this, a struct
# layout change leaves stale objects linked against the old layout -> garbage
# field reads / segfaults at runtime. -MP adds phony header targets so a
# deleted header doesn't break the build.
DEPFLAGS := -MMD -MP
CFLAGS   += $(DEPFLAGS)
LDLIBS  := $(PKG_LIBS) $(MECAB_LIBS) -lpthread -lm -ldl $(ICONV_LIB) -lz

OBJ := obj
BIN := bin/japanosint

CORE_SRC := $(wildcard core/*.c)
LIB_SRC  := $(wildcard lib/*.c)
SRC_SRC  := $(wildcard collectors/sources/*.c)
SRCS     := main.c registry.c $(CORE_SRC) $(LIB_SRC) $(SRC_SRC)
OBJS     := $(patsubst %.c,$(OBJ)/%.o,$(SRCS)) \
            $(OBJ)/third_party/cJSON.o $(OBJ)/third_party/sqlite3.o \
            $(OBJ)/third_party/mongoose.o

.PHONY: all clean selftest
all: $(BIN)

$(OBJ)/third_party/sqlite3.o: third_party/sqlite3.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -g $(DEPFLAGS) $(SQLITE_DEFS) -c $< -o $@

$(OBJ)/third_party/cJSON.o: third_party/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/third_party/mongoose.o: third_party/mongoose.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -g $(DEPFLAGS) -Ithird_party -c $< -o $@

$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@echo "built $@"

selftest: $(BIN)
	./$(BIN) --selftest

clean:
	rm -rf $(OBJ) bin

# Pull in the auto-generated header dependencies (one .d per .o). The leading
# '-' silences the first-build case where they don't exist yet.
-include $(OBJS:.o=.d)
