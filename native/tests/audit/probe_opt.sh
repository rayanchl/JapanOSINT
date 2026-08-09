#!/usr/bin/env bash
# Does the --run segfault depend on optimisation level, ASAN, or on STALE
# objects sitting in obj/ from an earlier header layout?
cd "$(dirname "$0")/../.." || exit 1
export JO_SCHEMA="$PWD/core/schema.sql"
export ASAN_OPTIONS=detect_leaks=0

run_with() {
  local bin="$1"; shift
  [ -x "$bin" ] || { printf '  %-28s (missing)\n' "$(basename "$bin")"; return; }
  rm -f /tmp/p.db /tmp/p.db-wal /tmp/p.db-shm
  JO_DB=/tmp/p.db "$bin" --run "$@" >/tmp/p.txt 2>&1
  local rc=$?
  printf '  %-28s rc=%-4s sched=[%s]\n' "$(basename "$bin")" "$rc" \
    "$(grep -o '\[sched\].*' /tmp/p.txt | head -1)"
}

for args in "ASN_LOOKUP 8.8.8.8" "EARTHQUAKE_MONITOR" "DNS_RECORDS google.com"; do
  echo "== $args"
  # shellcheck disable=SC2086
  run_with ./bin/japanosint $args          # incremental obj/
  # shellcheck disable=SC2086
  run_with ./bin/japanosint-clean $args    # from-scratch obj-clean/
  # shellcheck disable=SC2086
  run_with ./bin/japanosint-asan $args     # ASAN
done
