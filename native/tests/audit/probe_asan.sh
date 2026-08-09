#!/usr/bin/env bash
# Run one source under ASAN and print the fault report.
# The ASAN build overrides CFLAGS and so loses -DJO_REPO_ROOT; JO_SCHEMA
# supplies the schema path explicitly. Leak reports are off — we want faults.
cd "$(dirname "$0")/../.." || exit 1
export JO_SCHEMA="$PWD/core/schema.sql"
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0:print_stacktrace=1
rm -f /tmp/asan.db /tmp/asan.db-wal /tmp/asan.db-shm
JO_DB=/tmp/asan.db ./bin/japanosint-asan --run "$@" 2>&1 \
  | grep -vE '^\[db\] migrated' \
  | sed -n '/ERROR: AddressSanitizer/,/^SUMMARY/p;/\[sched\]/p' \
  | head -45
