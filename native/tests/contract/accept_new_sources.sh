#!/bin/bash
# tests/contract/accept_new_sources.sh — the acceptance gate for newly added
# collectors.
#
# A source is not "added" because a .c file exists and the tree compiles. It is
# added when it demonstrably fetches real data. This runs the whole chain:
#
#   1. integrated build (the Makefile globs collectors/sources/*.c)
#   2. duplicate-id check   — registry.c warns; a clean fleet prints none
#   3. registration check   — every new .id actually reached the registry
#   4. contract harness     — run each new source and judge the rows it wrote
#
# Usage:
#   tests/contract/accept_new_sources.sh 'av_ cert_ cyi_ ...'   # file prefixes
#   tests/contract/accept_new_sources.sh --ids-file ids.txt
set -u
cd "$(dirname "$0")/../.." || exit 1     # -> native/

OUTDIR="${OUTDIR:-tests/contract}"
JOBS="${JOBS:-8}"
TIMEOUT="${TIMEOUT:-120}"

echo "=== 1. build ==="
if ! make -j"$(nproc)" > /tmp/accept_build.log 2>&1; then
  echo "BUILD FAILED — first errors:"
  grep -E "error:" /tmp/accept_build.log | head -30
  exit 2
fi
warn=$(grep -cE "warning:" /tmp/accept_build.log)
echo "build OK ($warn warnings)"

echo
echo "=== 2. duplicate ids ==="
# registry_add prints on collision; a duplicate id is silently unreachable
# via registry_get/--run/the OSINT dispatcher, so this must be zero.
dup=$(JO_DB=/tmp/accept_reg.db ./bin/japanosint --list-sources 2>&1 >/dev/null \
      | grep -c "DUPLICATE id")
echo "duplicate ids: $dup"
[ "$dup" -gt 0 ] && JO_DB=/tmp/accept_reg.db ./bin/japanosint --list-sources 2>&1 >/dev/null \
      | grep "DUPLICATE id" | head -20

echo
echo "=== 3. collect new ids ==="
if [ "${1:-}" = "--ids-file" ]; then
  IDS_FILE="$2"
else
  PREFIXES="${1:?usage: accept_new_sources.sh '<prefix> <prefix> ...' | --ids-file F}"
  FILES=""
  for p in $PREFIXES; do FILES="$FILES collectors/sources/${p}*.c"; done
  # shellcheck disable=SC2086
  grep -hoE '\.id *= *"[^"]+"' $FILES 2>/dev/null \
    | sed 's/.*= *"//; s/"//' | sort -u > /tmp/new_ids.txt
  IDS_FILE=/tmp/new_ids.txt
fi
echo "ids declared in new files: $(wc -l < "$IDS_FILE")"

# Registration check: a REGISTER_SOURCE that never ran (or a source dropped by
# the registry cap) leaves an id in the source but absent from the binary.
JO_DB=/tmp/accept_reg.db ./bin/japanosint --list-sources 2>/dev/null \
  | cut -d' ' -f1 | sort -u > /tmp/registered.txt
missing=$(comm -23 "$IDS_FILE" /tmp/registered.txt)
if [ -n "$missing" ]; then
  echo "!! declared but NOT registered:"; echo "$missing" | head -20
fi

echo
echo "=== 4. contract harness (this makes real network calls) ==="
python3 tests/contract/source_contract.py \
  --ids-file "$IDS_FILE" --jobs "$JOBS" --timeout "$TIMEOUT" \
  --out "$OUTDIR/verdicts_new.tsv" \
  ${BASELINE:+--baseline "$BASELINE"} \
  ${ENTITIES:+--entities-file "$ENTITIES"}
rc=$?

echo
echo "verdicts -> $OUTDIR/verdicts_new.tsv"
exit $rc
