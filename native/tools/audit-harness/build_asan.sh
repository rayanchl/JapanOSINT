#!/usr/bin/env bash
# Parallel ASAN build into obj-asan/bin-asan so it never clobbers the normal
# build the audit harness runs against.
set -e
cd "$(dirname "$0")/../.." || exit 1
make -j"$(nproc)" OBJ=obj-asan BIN=bin/japanosint-asan \
  CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wno-unused-parameter -pthread" \
  2>&1 | grep -vE '^(cc |In file|  *[0-9]+ \||  *\||.*note:|.*warning:|.*\^)' | tail -20
echo "asan build done"
