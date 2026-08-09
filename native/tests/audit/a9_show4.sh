#!/usr/bin/env bash
N="$1"; shift
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a9_out4 || exit 1
for f in "$@"; do
  echo "##### $f"
  sed -n "1,${N}p" "$f.txt"
done
