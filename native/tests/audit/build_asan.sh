#!/usr/bin/env bash
# Parallel ASAN build into obj-asan/bin-asan so it never clobbers the normal
# build the audit harness runs against.
#
# A FAILED BUILD MUST BE LOUD. This used to be `set -e` plus
#     make … 2>&1 | grep -vE '…' | tail -20
#     echo "asan build done"
# and every part of that hid the failure: make is FIRST in a pipeline, so the
# shell's status is tail's, which is always 0; without `pipefail` `set -e`
# never fires; and the unconditional echo then announced success. The result
# was an auditor sanitizing a bin/japanosint-asan that could be hours stale —
# the same defect agent_build.sh:14-18 documents and launch.sh was fixed for.
# make's own status decides now, and the binary's mtime is printed so a stale
# artefact is visible at a glance.
set -euo pipefail
cd "$(dirname "$0")/../.." || exit 1

BIN=bin/japanosint-asan
LOG=obj-asan/build.log
mkdir -p obj-asan

# ASan goes in CC, not CFLAGS — the SAME construction the `asan` Makefile target
# uses, and for the reason that target's own comment gives: a single TU compiled
# without the flag makes the report meaningless. The third_party rules for
# sqlite3.o and mongoose.o hardcode `$(CC) -O2 -g …` and IGNORE $(CFLAGS), so
# passing the flag there left this harness blind inside SQLite and inside the
# mongoose HTTP request parser — the first code to touch attacker-controlled
# bytes, and precisely where an ASan report would matter most. Putting it in CC
# reaches every rule. Overriding CFLAGS also silently dropped -Wextra, so the
# warning summary this script prints was narrower than a normal build's.
if ! make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" \
       OBJ=obj-asan BIN="$BIN" \
       CC="${CC:-cc} -fsanitize=address -fno-omit-frame-pointer" \
       >"$LOG" 2>&1; then
  echo "asan build FAILED — $BIN was NOT updated, do not trust a run against it."
  grep -E 'error|Error|undefined reference|No rule to make' "$LOG" | tail -25 || true
  echo "full log: $LOG"
  exit 1
fi
if [ ! -x "$BIN" ]; then
  echo "asan build FAILED — make succeeded but $BIN is missing."
  exit 1
fi
grep -E 'warning:' "$LOG" | tail -20 || true
echo "asan build OK -> $BIN (linked $(date -r "$BIN" '+%H:%M:%S'))"
