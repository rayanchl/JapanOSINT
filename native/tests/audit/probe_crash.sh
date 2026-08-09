#!/usr/bin/env bash
# Narrow down which argument shape makes --run die without printing [sched].
cd "$(dirname "$0")/../.." || exit 1
BINARY="${1:-./bin/japanosint}"
try() {
  rm -f /tmp/v.db /tmp/v.db-wal /tmp/v.db-shm
  JO_DB=/tmp/v.db "$BINARY" --run "$@" >/tmp/o.txt 2>&1
  local rc=$?
  local sched
  sched=$(grep -c '\[sched\]' /tmp/o.txt)
  printf 'rc=%-4s sched_lines=%s args=[%s]\n' "$rc" "$sched" "$*"
  if [ "$rc" -ge 128 ]; then
    grep -E 'ERROR|SUMMARY|#[0-9]+ ' /tmp/o.txt | head -25
  fi
}
try ASN_LOOKUP
try ASN_LOOKUP 8.8.8.8
try EARTHQUAKE_MONITOR
try jma-earthquake
