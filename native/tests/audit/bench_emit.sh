#!/usr/bin/env bash
# Investigation-only: build + run the emit() microbenchmark. Touches nothing
# in the tree except its own scratch files under /tmp.
set -e
cd "$(dirname "$0")"
cc -O2 -o /tmp/bench_emit bench_emit.c ../../third_party/sqlite3.c \
   -DSQLITE_ENABLE_FTS5 -DSQLITE_THREADSAFE=1 \
   -lpthread -ldl -lm 2>&1 | grep -E 'error' || true
/tmp/bench_emit "${1:-20000}" /tmp
