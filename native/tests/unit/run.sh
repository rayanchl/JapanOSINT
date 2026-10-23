#!/bin/bash
# tests/unit/run.sh — build and run the unit tests against a scratch database.
#
# Each test #includes the .c it is testing so it can reach static functions, so
# the link line takes every object EXCEPT main.o (its own main) and the object
# of the file under test (duplicate symbols).
#
# Usage:  make -j && tests/unit/run.sh
#
# OBJDIR/CC are overridable so the same tests can run against a sanitizer
# build, which is the point of having them for a threaded change:
#   make CC="cc -fsanitize=thread" OBJ=obj-tsan BIN=bin/japanosint-tsan -j
#   OBJDIR=obj-tsan CC="cc -fsanitize=thread" tests/unit/run.sh
set -u
cd "$(dirname "$0")/../.." || exit 1     # -> native/

OBJDIR="${OBJDIR:-obj}"
CC="${CC:-cc}"
# ThreadSanitizer aborts with "unexpected memory mapping" under the ASLR
# entropy most current kernels default to; setarch -R pins it. Harmless for
# non-sanitizer runs, so it is applied whenever available.
RUNNER="${RUNNER:-}"
if [ -z "$RUNNER" ] && command -v setarch >/dev/null 2>&1; then
  setarch -R true >/dev/null 2>&1 && RUNNER="setarch -R"
fi
[ -d "$OBJDIR" ] || { echo "no $OBJDIR/ — run make first"; exit 2; }

CFLAGS="-O1 -g -Wall -Wextra -Wno-unused-parameter -pthread -Ithird_party"
# The same two quote-include paths the main Makefile passes. A test that
# #includes a COLLECTOR reaches `#include "source.h"` and `#include
# "_jp_osint.inc"`, and "" search starts in the directory of the including
# file — collectors/sources/ — not in native/. Without these the collector
# under test does not compile at all, which reads as a broken test rather than
# as a missing -I. Additive: nothing that built before builds differently.
CFLAGS="$CFLAGS -iquote . -iquote collectors/sources"
CFLAGS="$CFLAGS -DJO_REPO_ROOT=\"$(cd .. && pwd)\""
# Ask the Makefile for the link line rather than re-deriving it. The previous
# `pkg-config --libs libcurl openssl 2>/dev/null` had none of the fallbacks the
# Makefile spends fifteen lines explaining are necessary on macOS: openssl@3 is
# keg-only so pkg-config often resolves nothing, and Apple's libcurl is built
# without the WebSocket support lib/ws.c needs. Worse, the 2>/dev/null turned a
# total failure to resolve into an EMPTY STRING, so the failure surfaced as
# undefined EVP_* symbols at link instead of a clear message. Duplicating the
# resolution is why the two drifted; there is now one copy.
LDLIBS="$(make -s -C "$(dirname "$0")/../.." print-ldlibs)"
if [ -z "$LDLIBS" ]; then
  echo "FAILED: could not resolve link libraries via 'make print-ldlibs'." >&2
  echo "Check that libcurl, openssl and mecab are installed and discoverable." >&2
  exit 1
fi
CFLAGS="$CFLAGS $(make -s -C "$(dirname "$0")/../.." print-cflags)"

SCRATCH="${TMPDIR:-/tmp}/jo-unit-$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

# ORPHAN OBJECTS ARE NOT PART OF THIS TREE.
#
# This used to link `find $OBJDIR -name '*.o'` — EVERY object, including ones
# whose .c was renamed or moved months ago and which `make` itself never links
# (it derives its object list from the current sources). A moved collector then
# registers its ids TWICE and the binary reports `[registry] DUPLICATE id …`,
# a phantom that does not exist in the repo: `grep` finds exactly one
# definition and the real build is clean. Measured 2026-09-13 on a TSan tree:
# `collectors/sources/anomaly_triage.o` beside `collectors/pod/anomaly_triage.o`
# and twelve more like it, which made the service-index test see 1,846 pivots
# where the source tree has 1,838 — read as a product regression, and it was a
# stale directory. So: keep an object only if its source still exists.
prune_orphans() {                       # stdin: object paths, stdout: the live ones
  local o rel
  while read -r o; do
    rel=${o#"$OBJDIR"/}
    rel=${rel%.o}.c
    if [ -f "$rel" ]; then printf '%s\n' "$o"; else
      printf 'stale object ignored (no %s): %s\n' "$rel" "$o" >&2
    fi
  done
}

under_of() {                            # a test's #included .c, as an object path
  grep -oE '#include "\.\./\.\./[a-z_/]+\.c"' "$1" |
    head -1 | sed 's|#include "../../||; s|"$||; s|\.c$|.o|'
}

# ONE PARTIAL LINK INSTEAD OF 84 FULL ONES.
#
# Every test links the whole tree, so the link line used to be ~1,860 objects
# and the suite spent its time in ld rather than in the tests: measured
# 2026-10-23 on this tree, 35.7 s per test — about fifty minutes of pure
# linking, which is what made the TSan job hit its 90-minute ceiling.
#
# `ld -r` merges the objects NO test excludes into one relocatable object once
# (1.2 s), and each test then links that plus the 53 objects some test does
# exclude: 2.7 s, a 13x saving, for a binary 4 KB different from the old one.
#
# It is a PARTIAL link, not an archive, and that distinction is the whole
# reason this is safe. An `ar` archive measured 0.27 s and produced a binary
# 10.5 MB against 128.3 MB: archive members are pulled only to satisfy an
# undefined symbol, and this tree's collectors self-register from constructors
# that nothing references, so the test ran against an EMPTY registry (`[db]
# seeded sources from registry (0 new … 58047 stale pruned)`) and still printed
# "all passed", because the one it was checked with needs only *some* scheduled
# row. A partial link keeps every section and every .init_array entry, so the
# registry seeds the same 58,047 rows it does without this.
#
# It also keeps the duplicate-symbol check the prune_orphans note above exists
# for — verified rather than assumed: `ld -r` over two objects defining one
# function reports "multiple definition" and exits 1, exactly as the final link
# does. So a stale object that redefines a symbol still fails the build, once
# here instead of 84 times. A duplicate REGISTRATION (same id, different
# symbol) is not a link error either way and is still caught by prune_orphans.
live_objs="$SCRATCH/live.txt"
find "$OBJDIR" -name '*.o' ! -name 'main.o' | prune_orphans | sort -u > "$live_objs"
for src in tests/unit/test_*.c; do under_of "$src"; done |
  sed '/^$/d' | sort -u | sed "s|^|$OBJDIR/|" > "$SCRATCH/under.txt"
grep -vxFf "$SCRATCH/under.txt" "$live_objs" > "$SCRATCH/common.txt"

COMMON=""
# shellcheck disable=SC2046
if ld -r -o "$SCRATCH/common.o" $(cat "$SCRATCH/common.txt") 2>"$SCRATCH/ldr.err"; then
  COMMON="$SCRATCH/common.o"
  echo "--- partial link: $(wc -l < "$SCRATCH/common.txt") objects -> common.o," \
       "$(wc -l < "$SCRATCH/under.txt") kept separate ---"
else
  # Not fatal: fall back to the old full object list. A real duplicate symbol
  # then fails every test's own link instead, so nothing is hidden by this.
  echo "--- partial link unavailable, linking every object per test ---" >&2
  sed -n '1,5p' "$SCRATCH/ldr.err" >&2
fi

fail=0
for src in tests/unit/test_*.c; do
  name=$(basename "$src" .c)
  # Which object does this test include? (duplicate symbols if also linked.)
  under=$(under_of "$src")
  if [ -n "$COMMON" ]; then
    objs="$COMMON $(grep -vxF "$OBJDIR/$under" "$SCRATCH/under.txt")"
  else
    objs=$(grep -vxF "$OBJDIR/$under" "$live_objs")
  fi

  echo "--- $name ($OBJDIR, excludes ${under:-<none>}) ---"
  # shellcheck disable=SC2086
  if ! $CC $CFLAGS "$src" $objs -o "$SCRATCH/$name" $LDLIBS 2>"$SCRATCH/$name.cc"; then
    echo "BUILD FAILED"; sed -n '1,20p' "$SCRATCH/$name.cc"; fail=1; continue
  fi
  JO_DB="$SCRATCH/$name.db" $RUNNER "$SCRATCH/$name"
  rc=$?
  [ $rc -eq 0 ] || { echo "FAILED (exit $rc)"; fail=1; }
  rm -f "$SCRATCH/$name" "$SCRATCH/$name.db"
done

# tools/cleanup_stale_uids.py deletes rows from a live database. Its self-test
# builds a scratch DB from core/schema.sql and pins that only rows whose
# replacement is proven go, and nothing else does.
echo "--- cleanup_stale_uids self-test ---"
python3 tools/cleanup_stale_uids.py --self-test || { echo "FAILED"; fail=1; }

# tools/measure_thresholds.py is the harness for open item E1 — the eleven
# embedding thresholds that have never been measured on the production model.
# It needs a real embedding server to measure anything and refuses a stub for
# that, so what runs here is its own self-test: a stub server in-process, which
# tests THE TOOL. It pins the one thing that would silently invert a reading —
# that an L2 distance is swept the other way round from a cosine — and that it
# reads all eleven keys out of core/thresholds.c rather than keeping a copy.
echo "--- measure_thresholds self-test ---"
python3 tools/measure_thresholds.py --self-test || { echo "FAILED"; fail=1; }

exit $fail
