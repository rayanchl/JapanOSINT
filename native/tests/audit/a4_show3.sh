#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a4_fix
N="${N:-40}"
for f in "$@"; do
  echo "##### $f"
  sed -n '/--- row 1 ---/,$p' "$f.txt" | head -"$N"
  echo
done
