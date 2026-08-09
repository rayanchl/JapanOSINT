#!/usr/bin/env bash
# Full from-scratch -O2 build into obj-clean/bin-clean, using the Makefile's
# own CFLAGS (so JO_REPO_ROOT / mecab / dep-tracking are all intact).
set -e
cd "$(dirname "$0")/../.." || exit 1
rm -rf obj-clean
make -j"$(nproc)" OBJ=obj-clean BIN=bin/japanosint-clean 2>&1 | tail -3
echo "clean build done"
