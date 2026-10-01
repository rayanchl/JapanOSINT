#!/usr/bin/env bash
# Full from-scratch -O2 build into obj-clean/bin-clean, using the Makefile's
# own CFLAGS (so JO_REPO_ROOT / mecab / dep-tracking are all intact).
#
# This is the script that answers "does a clean tree still compile", so it is
# the one script that must never answer yes by default. It used to be `set -e`
# plus `make … | tail -3` followed by an unconditional `echo "clean build
# done"`: make sits first in a pipeline, so the status the shell saw was
# tail's (always 0), `set -e` without `pipefail` never fired, and the echo
# reported success for a build that had failed. Same fix as agent_build.sh and
# build_asan.sh — read make's own status, then prove the binary exists.
set -euo pipefail
cd "$(dirname "$0")/../.." || exit 1

BIN=bin/japanosint-clean
LOG=obj-clean-build.log

rm -rf obj-clean
if ! make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" \
       OBJ=obj-clean BIN="$BIN" >"$LOG" 2>&1; then
  echo "clean build FAILED — a from-scratch compile of this tree does not work."
  grep -E 'error|Error|undefined reference|No rule to make' "$LOG" | tail -25 || true
  echo "full log: $LOG"
  exit 1
fi
if [ ! -x "$BIN" ]; then
  echo "clean build FAILED — make succeeded but $BIN is missing."
  exit 1
fi
# A clean build is also the only place the whole tree's warnings are visible;
# an incremental build shows only what it recompiled.
echo "clean build OK -> $BIN ($(grep -c 'warning:' "$LOG" || true) warnings)"
grep 'warning:' "$LOG" | sed -E 's/.*\[(-W[a-z-]+)\]/\1/' | sort | uniq -c | sort -rn | head
