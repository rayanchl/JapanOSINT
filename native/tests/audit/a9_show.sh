#!/usr/bin/env bash
# a9_show.sh <n-lines> <id...>
N="$1"; shift
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a9_out || exit 1
for f in "$@"; do
  echo "##### $f"
  sed -n "1,${N}p" "$f.txt"
done
