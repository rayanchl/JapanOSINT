#!/usr/bin/env bash
# Per-auditor build slot: agent_build.sh <slot>
#
# Ten auditors sharing obj/ would race the same .o files and the same link, so
# each gets obj-<slot>/ + bin/japanosint-<slot>. The slot is seeded by copying
# the already-built obj/ (52 MB) so the first build is incremental, not a
# 3-minute cold build times ten.
#
# The .d dependency files inside the copy name obj/... targets. Left alone they
# would declare rules for the WRONG tree, so make would not know that editing a
# header must rebuild this slot's objects — which is exactly the stale-object
# bug that made --run segfault before this audit. Rewrite them on copy.
#
# A failed build MUST be loud. The previous version piped make through
# `grep ... || true` and then tested `-x` on the binary, so a broken link
# printed "OK" while the auditor went on to verify against a binary that could
# be 40 minutes stale. Now make's own exit status decides, and the mtime of the
# binary is printed so staleness is visible at a glance.
set -euo pipefail
SLOT="${1:?usage: agent_build.sh <slot>}"
cd "$(dirname "$0")/../.." || exit 1
OBJ="obj-$SLOT"
BIN="bin/japanosint-$SLOT"
if [ ! -d "$OBJ" ]; then
  echo "[slot $SLOT] seeding $OBJ from obj/ ..."
  cp -a obj "$OBJ"
  find "$OBJ" -name '*.d' -exec sed -i "s#\bobj/#${OBJ}/#g" {} +
fi
LOG="$OBJ/build.log"
if ! make -j4 OBJ="$OBJ" BIN="$BIN" >"$LOG" 2>&1; then
  echo "[slot $SLOT] BUILD FAILED — $BIN was NOT updated, do not trust a run against it."
  grep -E 'error|Error|undefined reference|No rule to make' "$LOG" | tail -25 || true
  echo "[slot $SLOT] full log: $LOG"
  exit 1
fi
grep -E 'warning: .*(uninitial|overflow|format)' "$LOG" | head -20 || true
if [ ! -x "$BIN" ]; then
  echo "[slot $SLOT] BUILD FAILED — make succeeded but $BIN is missing."
  exit 1
fi
echo "[slot $SLOT] OK -> $BIN (linked $(date -r "$BIN" '+%H:%M:%S'))"
