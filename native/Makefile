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

# Tunable half: `make CFLAGS=-O0 -j` (or CFLAGS=-O0 in the environment) is a
# legitimate thing to want.
CFLAGS  ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -pthread
# Non-negotiable half. A command-line `CFLAGS=...` outranks a plain `+=` and
# would silently wipe these — dropping -DJO_REPO_ROOT (grammars/prompts stop
# resolving), -Ithird_party (cJSON/sqlite3 headers vanish) and, worst,
# -MMD -MP, which disables the .d mechanism below precisely when someone is
# doing an unusual build. `override` keeps the correctness flags no matter
# what the caller passes, while still letting them change the optimisation
# level, which is the only thing they actually wanted to change.
override CFLAGS += -DJO_REPO_ROOT='"$(REPO_ROOT)"' $(PKG_CFLAGS) $(MECAB_CFLAGS) -Ithird_party
# Auto-generate header dependency files (.d) next to each .o so editing a
# header (e.g. source.h, which defines the ABI-critical source_def struct)
# forces a recompile of every TU that includes it. Without this, a struct
# layout change leaves stale objects linked against the old layout -> garbage
# field reads / segfaults at runtime. -MP adds phony header targets so a
# deleted header doesn't break the build.
DEPFLAGS := -MMD -MP
override CFLAGS += $(DEPFLAGS)
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

.PHONY: all clean selftest unit tsan tsan-test tsan-sched asan asan-test \
        lint-sources lint-baseline source-count
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
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@echo "built $@"

selftest: $(BIN)
	./$(BIN) --selftest

# Unit/contract tests. Each test #includes the .c it exercises so it can reach
# static functions, so run.sh links every object but main.o and that file's.
unit: $(BIN)
	tests/unit/run.sh

# ThreadSanitizer build + the same tests under it. The scheduler is a worker
# pool (core/scheduler.c) and OSINT dispatch fans out (core/pipeline.c), so
# "it compiles and doesn't crash once" is not evidence of thread safety —
# every race this pair introduced was found here and nowhere else. TSan needs
# its own object tree because every TU must carry -fsanitize=thread.
#
#   make tsan            # build bin/japanosint-tsan
#   make tsan-test       # build it and run the unit tests under it
#   make tsan-sched      # 90 s of the real scheduler pool under it
#
# TSAN_OBJ/TSAN_BIN/TSAN_SECS are overridable so the tree can be put on a fast
# filesystem (this repo is routinely checked out on a 9p/DrvFs mount, where the
# 2100-file object tree dominates the build) and so the soak can be run longer
# than the default when hunting a probabilistic race:
#   make tsan-sched TSAN_OBJ=$HOME/tsan/obj TSAN_BIN=$HOME/tsan/bin/jo TSAN_SECS=300
TSAN_OBJ  ?= obj-tsan
TSAN_BIN  ?= bin/japanosint-tsan
TSAN_SECS ?= 90
tsan:
	$(MAKE) CC="$(CC) -fsanitize=thread" OBJ=$(TSAN_OBJ) BIN=$(TSAN_BIN)

tsan-test: tsan
	OBJDIR=$(TSAN_OBJ) CC="$(CC) -fsanitize=thread" tests/unit/run.sh

# A CLEAN RUN AND A RUN THAT NEVER HAPPENED MUST NOT LOOK THE SAME.
#
# This used to be `... | tee .run/tsan-sched.log | grep -E ... || true` followed
# by a grep -c of that log. tee truncates the log before the binary is even
# execed, `||`/pipelines hide the exit status, and grep -c of an empty file is
# 0 — so a missing setarch, a binary that was never built, and an immediate
# crash ALL printed "--- races: 0 ---", i.e. the strongest result this target
# can report. The whole point of the target is to be believed, so it now
# proves the run happened before it counts anything:
#   - the binary must exist and be executable;
#   - `setarch -R` (ASLR off — TSan's shadow mapping needs it on kernels with
#     a large mmap_rnd_bits) must actually work, checked separately so its
#     absence is named rather than silently producing an empty log;
#   - the log must be non-empty AND contain scheduler output (`run rc=` or
#     `[sched]`), otherwise the target FAILS instead of reporting zero;
#   - timeout's 124 (the expected "soak ran to the end") is the success case;
#     any other non-zero exit is reported with its code, since a TSan
#     halt_on_error abort or a crash is itself the finding.
tsan-sched: tsan
	@mkdir -p .run
	@test -x $(abspath $(TSAN_BIN)) || \
	  { echo "tsan-sched: $(TSAN_BIN) missing or not executable — build failed"; exit 1; }
	@setarch -R true 2>/dev/null || \
	  { echo "tsan-sched: 'setarch -R' unavailable; TSan cannot map its shadow with ASLR on"; exit 1; }
	@rm -f .run/tsan-sched.log
	@JO_DB=.run/tsan-sched.db JO_SCHED_WORKERS=$${JO_SCHED_WORKERS:-8} \
	  JO_SCHED_STAGGER_SEC=$${JO_SCHED_STAGGER_SEC:-0} \
	  TSAN_OPTIONS=halt_on_error=0 \
	  setarch -R timeout $(TSAN_SECS) $(abspath $(TSAN_BIN)) --sched \
	  > .run/tsan-sched.log 2>&1; \
	  rc=$$?; \
	  if [ $$rc -ne 124 ] && [ $$rc -ne 0 ]; then \
	    echo "tsan-sched: the run exited $$rc before the $(TSAN_SECS)s deadline — see .run/tsan-sched.log"; \
	  fi
	@grep -E "run rc=|ThreadSanitizer" .run/tsan-sched.log || true
	@lines=$$(wc -l < .run/tsan-sched.log); \
	 if [ "$$lines" -eq 0 ] || ! grep -qE "run rc=|\[sched\]" .run/tsan-sched.log; then \
	   echo "tsan-sched: FAILED — the log has $$lines line(s) and no scheduler output;"; \
	   echo "            the soak did not run, so 0 races would be a lie."; \
	   exit 1; \
	 fi; \
	 echo "--- races: $$(grep -c 'WARNING: ThreadSanitizer' .run/tsan-sched.log) in $$lines log lines ---"

# AddressSanitizer build + the same tests under it. The heap bugs this codebase
# actually ships are use-after-free and over-read inside the collector tree
# (freed cJSON re-read, an unterminated buffer walked past its end) — none of
# which crash reliably, so "it ran and produced rows" is not evidence of
# memory safety. Like TSan this needs its OWN object tree: ASan changes the
# layout of every allocation, so a single TU compiled without the flag makes
# the report meaningless (or the link fail outright).
# -fno-omit-frame-pointer is what turns the report from addresses into a stack.
#
#   make asan            # build bin/japanosint-asan
#   make asan-test       # build it and run the unit tests under it
ASAN_FLAGS := -fsanitize=address -fno-omit-frame-pointer
asan:
	$(MAKE) CC="$(CC) $(ASAN_FLAGS)" OBJ=obj-asan BIN=bin/japanosint-asan

asan-test: asan
	OBJDIR=obj-asan CC="$(CC) $(ASAN_FLAGS)" tests/unit/run.sh

# Mechanical checks over the collector tree (duplicate ids, orphaned registry
# rows, the quarantine-on-honest-empty return shape, unguarded snprintf
# accumulators, …). Baselined in tools/lint_baseline.json so it is green today
# and can only ratchet down — see tools/lint_sources.py's header.
lint-sources:
	python3 tools/lint_sources.py

# Re-record the baseline after a batch of fixes lands.
lint-baseline:
	python3 tools/lint_sources.py --write-baseline

# The one true source count. Every number written into a doc goes stale; this
# is how you get the current one (macro-expanded, so it is not a grep count).
source-count:
	@python3 tools/lint_sources.py --count

# obj-* covers every sanitizer/agent-slot tree (obj-asan, obj-tsan, obj-a0…),
# which is where the hundreds of orphaned megabytes lived: clean removed only
# $(OBJ) and obj-tsan, so every other tree survived forever, unignored and
# invisible. Quoted+globbed via find so a slot dir with a space in its name
# (obj- with a trailing space really existed) is removed too.
clean:
	rm -rf $(OBJ) bin
	@find . -maxdepth 1 -type d -name 'obj-*' -exec rm -rf {} +

# Pull in the auto-generated header dependencies (one .d per .o). The leading
# '-' silences the first-build case where they don't exist yet.
-include $(OBJS:.o=.d)
