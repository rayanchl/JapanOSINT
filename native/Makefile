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
# -iquote decouples a collector's #include lines from its DEPTH in the tree.
# Every collector used to say #include "../../source.h", which encodes "I am
# exactly two directories below native/". That made the flat
# collectors/sources/ layout load-bearing: moving one file into a subdirectory
# broke ~3,000 include lines, so the tree could never be organised without a
# mechanical rewrite of all of them, and a per-destination rewrite would have
# to be redone on the next move too.
#
# With these, a collector writes what it means -- #include "source.h",
# "lib/feedlib.h", "core/httpclient.h" -- and it resolves from any depth,
# forever. -iquote (not -I) because this must apply to "..." includes only and
# be searched AFTER the including file's own directory, so a sibling header
# still wins over a same-named one at the root.
#   .                    native/ itself: source.h, lib/*, core/*, third_party/*
#   collectors/sources   the shared .inc/.h siblings (_jp_osint.inc,
#                        _verified_macros.inc, trn_common.inc, social_fuse.h …)
#                        which 200+ collectors include by bare name.
override CFLAGS += -DJO_REPO_ROOT='"$(REPO_ROOT)"' $(PKG_CFLAGS) $(MECAB_CFLAGS) \
                   -Ithird_party -iquote . -iquote collectors/sources
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
# RECURSIVE on purpose. This was `wildcard collectors/sources/*.c`, which is a
# trap the moment anyone organises the 1,200+ collector files into
# subdirectories: a moved file silently leaves the link, the constructor that
# would have called REGISTER_SOURCE never runs, and the source simply stops
# existing at runtime — no error, no warning, and `make` still succeeds. The
# only symptom is a smaller number from `make source-count`, which nobody
# diffs. Any nesting under collectors/ is now picked up.
SRC_SRC  := $(shell find collectors -name '*.c' | sort)
SRCS     := main.c registry.c $(CORE_SRC) $(LIB_SRC) $(SRC_SRC)
OBJS     := $(patsubst %.c,$(OBJ)/%.o,$(SRCS)) \
            $(OBJ)/third_party/cJSON.o $(OBJ)/third_party/sqlite3.o \
            $(OBJ)/third_party/mongoose.o $(OBJ)/third_party/sqlite-vec.o

.PHONY: all clean selftest unit authtest pagewalktest htmlparsetest tsan tsan-test tsan-sched asan asan-test \
        collectors-def collectors-def-check batch-gates \
        lint-sources lint-baseline source-count
all: $(BIN)

$(OBJ)/third_party/sqlite3.o: third_party/sqlite3.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -g $(DEPFLAGS) $(SQLITE_DEFS) -c $< -o $@

# sqlite-vec amalgamation (v0.1.9, MIT/Apache-2.0 — third_party/sqlite-vec.LICENSE-*).
# -DSQLITE_CORE: statically linked next to sqlite3.o, so it calls the real
# API instead of going through the sqlite3_api_routines loadable-extension
# shim (and needs no sqlite3ext.h). Built with the same flags as sqlite3.o —
# it is vendored code, not ours, so it is not held to -Wall -Wextra.
$(OBJ)/third_party/sqlite-vec.o: third_party/sqlite-vec.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -g $(DEPFLAGS) -DSQLITE_CORE -Ithird_party -c $< -o $@

$(OBJ)/third_party/cJSON.o: third_party/cJSON.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/third_party/mongoose.o: third_party/mongoose.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -g $(DEPFLAGS) -Ithird_party -c $< -o $@

$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# DELETING A COLLECTOR MUST RELINK. `$(BIN): $(OBJS)` only notices a
# prerequisite that is NEWER than the binary; one that disappeared from the
# list is not a prerequisite any more, so `make` said "up to date" and kept
# serving a binary that still registered the deleted sources — and
# registry-floor, --list-sources and every audit then measured a tree that no
# longer existed. The object list itself is now a prerequisite: rewritten on
# every run, but only REPLACED (new mtime) when its content changed, so an
# unchanged tree does not relink and an added or removed .c always does.
# (printf, not $(file ...): macOS still ships GNU make 3.81, which has no
# $(file) and would expand it to nothing.)
OBJLIST := $(OBJ)/.objlist
.PHONY: FORCE
FORCE:
$(OBJLIST): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' $(OBJS) > $@.tmp
	@if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv -f $@.tmp $@; fi

$(BIN): $(OBJS) $(OBJLIST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@echo "built $@"

selftest: $(BIN)
	./$(BIN) --selftest

# ── one source of truth for dependency resolution ───────────────────────────
# tests/unit/run.sh used to re-derive its own link line with a bare
# `pkg-config --libs libcurl openssl 2>/dev/null` — no Homebrew fallback, and
# the 2>/dev/null turned a total failure to resolve into an EMPTY STRING, so on
# macOS the build passed and `make unit` failed at link with undefined EVP_*
# symbols. These targets hand it the values computed above instead.
.PHONY: print-cflags print-ldlibs
print-cflags:
	@echo '$(PKG_CFLAGS) $(MECAB_CFLAGS)'
print-ldlibs:
	@echo '$(LDLIBS)'

# Exhaustive-use audit (docs/SOURCE_EXHAUSTIVENESS.md): scan every collector for
# the patterns that mean fetched data was discarded, and print every finding.
# GATED on the strict set — the hp tables (collectors/pivot/table/hp*_*.c) and
# the generated deep-record tables (collectors/feed/generated/hp1[0-9]_*.c):
# any finding there exits 1. Findings elsewhere in the tree are printed but do
# not change the exit status.
.PHONY: audit-sources
audit-sources:
	@python3 tests/audit_source_exhaustiveness.py \
	  --strict 'collectors/pivot/table/hp*_*.c' \
	  --strict 'collectors/feed/generated/hp1[0-9]_*.c'

# (The batch-14 `verify-candidates` / `regen-candidates` targets are gone. Batch
# 14 was promoted long ago — every csrc14_* file was replaced by a verified
# vsrc14_* one — and `regen-candidates` still ended in
# `gen_verified_sources.py --prefix csrc14 --unverified`: one stray `make`
# would have put ~1,000 never-probed sources back into the registry.)

# Offline unit test for the high-penetrancy record engine (lib/hpengine.c). Its
# own http_request stub replaces libcurl, so it needs no network and no key —
# see tests/hpengine_test.c.
.PHONY: hptest
hptest:
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/hpengine_test tests/hpengine_test.c lib/hpengine.c lib/datetok.c \
	  lib/csv.c lib/htmlparse.c lib/seenset.c lib/xlsx.c lib/zipread.c lib/jsonlist.c lib/pagewalk.c \
	  collectors/pivot/table/hp_uk_deep.c \
	  third_party/cJSON.c $(ICONV_LIB) -lz
	./bin/hpengine_test

# Offline unit test for the HTML attribute scanner (lib/htmlparse.c). Pure
# string work, no network: it pins the boundary rule that keeps `data-src=`
# from answering a lookup for `src` — see tests/htmlparse_test.c.
.PHONY: htmlparsetest
htmlparsetest:
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/htmlparse_test tests/htmlparse_test.c lib/htmlparse.c \
	  lib/seenset.c
	./bin/htmlparse_test

# Offline test for the paging + disclosure engine (lib/pagewalk.c) with a
# scripted upstream. It decides, for ~2,700 generated sources, whether we may
# keep asking an endpoint for more and what we must say when we stop — the
# first half is fabricated requests if it is wrong, the second half is the
# silent discard the module exists to end, and neither is visible from a
# compile. No network.
.PHONY: pagewalktest
pagewalktest:
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/pagewalk_test tests/pagewalk_test.c lib/pagewalk.c \
	  third_party/cJSON.c
	./bin/pagewalk_test

# Offline round-trip for the JWT public-key path (core/auth.c). Generates real
# RSA-2048 and P-256 keys, publishes them as JWKs, signs real JWTs and runs the
# real jwk_to_pkey/verify_asym — because the failure mode of a wrong public key
# is not a crash, it is "every legitimate token stops verifying", which no
# compile and no smoke test can see. No network, no key material on disk.
.PHONY: authtest
authtest:
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/auth_jwk_test tests/auth_jwk_test.c \
	  third_party/cJSON.c $(LDLIBS)
	./bin/auth_jwk_test

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
	$(MAKE) -j$(JOBS) CC="$(CC) -fsanitize=thread" OBJ=$(TSAN_OBJ) BIN=$(TSAN_BIN)

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
# Parallelism for the sanitizer sub-makes. Detected, not hardcoded, so this
# works on a 3-core macOS runner and a 4-core ubuntu one alike.
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
# -j: 1,289 translation units at ASan's 2-3x compile cost is a ~60 minute
# serial build, which is the entire CI budget for this job. The recursive make
# carries no jobserver of its own, so the flag has to be here.
asan:
	$(MAKE) -j$(JOBS) CC="$(CC) $(ASAN_FLAGS)" OBJ=obj-asan BIN=bin/japanosint-asan

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

# Regenerate core/collectors.def from the binary's OWN registry. The census
# used to mean: start a server on a scratch DB, GET /api/collectors, convert
# it to TSV, run the generator by hand, rebuild — a manual loop that let a
# stale OVERRIDES entry through once (round 4). Now:
#
#   make && make collectors-def && make      # the def is compiled in, so rebuild
#
# The census TSV is kept under docs/source-pages-revamp/ as the record the
# generator ran from. collectors-def-check runs the generator WITHOUT writing:
# it exits non-zero while any registered collector key has no def line.
CENSUS_DIR := ../docs/source-pages-revamp
collectors-def: $(BIN)
	@d=$$(date +%F); f=$(CENSUS_DIR)/collector-census-$$d.tsv; \
	./$(BIN) --collector-census > $$f && \
	python3 tools/gen_collectors_def.py $$f > core/collectors.def && \
	echo "core/collectors.def regenerated from $$f -- run make again to compile it in"
collectors-def-check: $(BIN)
	@mkdir -p .run && ./$(BIN) --collector-census > .run/census-check.tsv && \
	python3 tools/gen_collectors_def.py .run/census-check.tsv > /dev/null && \
	echo "collectors-def-check: every registered collector has a def line"

# Every manifest gate in one command, in the order they are meant to run:
# duplicate ids/endpoints against the tree, paging declared where the endpoint
# pages, reachability (rule 3), then the live probe. The probe's pass list is
# what gen_hp_batch.py --pass-ids takes.
#
#   make batch-gates MANIFEST='../docs/candidate-sources-batch46.*.txt'
#   python3 tools/gen_hp_batch.py ../docs/candidate-sources-batch46.*.txt \
#           --pass-ids .run/batch-gates/pass-ids.txt --prefix hp3b46 --batch 46 --outdir collectors/pivot/table
OUT        ?= .run/batch-gates
PROBE_JOBS ?= 8
batch-gates: $(BIN)
	@test -n "$(MANIFEST)" || { echo "usage: make batch-gates MANIFEST='../docs/candidate-sources-batchNN.*.txt' [OUT=dir] [PROBE_JOBS=8]"; exit 2; }
	@mkdir -p $(OUT)
	python3 tools/batch_exclusions.py --bin ./$(BIN) --check $(MANIFEST)
	python3 tools/audit_batch_pagination.py $(MANIFEST)
	python3 tools/audit_batch_reachable.py $(MANIFEST)
	python3 tools/probe_hp_batch.py $(MANIFEST) --jobs $(PROBE_JOBS) --report-only --out $(OUT)/probe.tsv --pass-ids $(OUT)/pass-ids.txt
	@echo "batch-gates: pass list at $(OUT)/pass-ids.txt ($$(wc -l < $(OUT)/pass-ids.txt) ids)"

# Assert the fleet did not shrink. `make source-count` is a number nobody
# diffs, which is exactly how a silently-dropped collector survives a refactor:
# the build succeeds, the binary links, and the only trace is a smaller count.
# Record the number before a move, assert it after:
#
#   make source-floor-record        # before
#   ...move files...
#   make source-floor               # after — fails if any source vanished
#
# registry-floor asks the BINARY, which is the only thing that knows. The
# source-floor above is a regex count over the tree (tools/lint_sources.py
# --count). It now counts HP_REGISTER_TABLE rows as well as REGISTER_SOURCE —
# it was blind to them once, and that blindness cost us: the hp spec table was
# a fixed 1,024-entry array, and 1,481 rows were being dropped at boot with
# source-floor passing the whole time. Nothing failed, /api/status showed
# nothing, and most of two batches was registered into nothing. A source text
# count still cannot see a row dropped AT REGISTRATION, so this one boots the
# binary.
#
# It also asserts that registration was CLEAN. A DUPLICATE id means the second
# definition is unreachable; an OVERFLOW means a row was discarded outright; a
# NEVER-RUNS row is registered under a collector that neither the scheduler nor
# is_entity_pivot() will pick (registry.c explains the shape, and it was hiding
# 16 live entity pivots). None should ever be non-zero, so none is merely
# reported.
#
# --list-sources boots the binary, which opens, migrates and seeds a database.
# It runs against a throwaway JO_DB: the default is $(REPO_ROOT)/data/japanmap.db
# — the developer's live database — and the `rm -rf data/japanmap.db*` this
# used to do ran in native/, where no database lives, so a local run migrated
# and re-seeded the live one instead of starting clean.
.PHONY: registry-floor registry-floor-record
registry-floor-record: $(BIN)
	@d=$$(mktemp -d "$${TMPDIR:-/tmp}/jo_regfloor.XXXXXX"); \
	 JO_DB="$$d/r.db" JO_FTS_REBUILD=0 ./$(BIN) --list-sources 2>/dev/null \
	   | wc -l | tr -d ' ' > tools/registry-floor.txt; \
	 rm -rf "$$d"
	@echo "recorded registry floor: $$(cat tools/registry-floor.txt)"

registry-floor: $(BIN)
	@test -f tools/registry-floor.txt || { echo "no floor recorded; run make registry-floor-record"; exit 1; }
	@d=$$(mktemp -d "$${TMPDIR:-/tmp}/jo_regfloor.XXXXXX"); \
	 JO_DB="$$d/r.db" JO_FTS_REBUILD=0 ./$(BIN) --list-sources 2>tools/.reg.log \
	   | wc -l | tr -d ' ' > tools/.reg.count; \
	 rm -rf "$$d"
	@now=$$(cat tools/.reg.count); floor=$$(cat tools/registry-floor.txt); \
	 dup=$$(grep -c DUPLICATE tools/.reg.log || true); \
	 ovf=$$(grep -c OVERFLOW tools/.reg.log || true); \
	 dead=$$(grep -c NEVER-RUNS tools/.reg.log || true); \
	 rm -f tools/.reg.log tools/.reg.count; \
	 if [ "$$now" -lt "$$floor" ]; then \
	   echo "REGISTRY REGRESSED: $$now < $$floor (floor in tools/registry-floor.txt)"; \
	   exit 1; \
	 fi; \
	 if [ "$$dup" -ne 0 ]; then \
	   echo "$$dup DUPLICATE id(s) — the second definition of each never runs"; exit 1; \
	 fi; \
	 if [ "$$ovf" -ne 0 ]; then \
	   echo "$$ovf row(s) DROPPED at registration — a table cap is eating sources"; exit 1; \
	 fi; \
	 if [ "$$dead" -ne 0 ]; then \
	   echo "$$dead row(s) NEVER RUN — interval 0 under a collector that is not"; \
	   echo "  \"osint\" and not an internal \"_\" lane, so neither the scheduler"; \
	   echo "  nor is_entity_pivot() reaches them (house rule 3)"; exit 1; \
	 fi; \
	 echo "registry ok: $$now >= $$floor, 0 duplicates, 0 dropped, 0 never-runs"

# The floor is committed (tools/source-floor.txt) so CI and a reviewer see the
# same number the author did.
.PHONY: source-floor source-floor-record
source-floor-record:
	@python3 tools/lint_sources.py --count | grep -oE '[0-9]+' | head -1 > tools/source-floor.txt
	@echo "recorded source floor: $$(cat tools/source-floor.txt)"

source-floor:
	@test -f tools/source-floor.txt || { echo "no floor recorded; run make source-floor-record"; exit 1; }
	@now=$$(python3 tools/lint_sources.py --count | grep -oE '[0-9]+' | head -1); \
	 floor=$$(cat tools/source-floor.txt); \
	 if [ "$$now" -lt "$$floor" ]; then \
	   echo "SOURCE COUNT REGRESSED: $$now < $$floor (floor in tools/source-floor.txt)"; \
	   echo "a collector stopped registering — most likely it moved out of the build glob"; \
	   exit 1; \
	 fi; \
	 echo "source floor ok: $$now >= $$floor"

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
