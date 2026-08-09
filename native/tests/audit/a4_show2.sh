#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a4_out2
for f in "$@"; do
  echo "##### $f"
  sed -n '/--- stderr ---/,/--- persisted/p' "$f.txt"
done
