#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a4_out
N="${N:-30}"
for f in "$@"; do
  echo "##### $f"
  sed -n '/--- stderr ---/,$p' "$f.txt" | head -"$N"
  echo
done
