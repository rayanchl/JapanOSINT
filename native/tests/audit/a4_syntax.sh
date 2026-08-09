#!/bin/bash
# compile-only syntax check for the files auditor a4 edited (no linking,
# writes to a scratch dir so obj/ and obj-a4/ are untouched)
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
CFLAGS="-O2 -g -Wall -Wextra -Wno-unused-parameter -pthread"
mkdir -p /tmp/a4syn
rc=0
for f in "$@"; do
  out=/tmp/a4syn/$(basename "$f" .c).o
  if ! gcc $CFLAGS -c "$f" -o "$out" 2>/tmp/a4syn/err.txt; then
    echo "FAIL $f"; cat /tmp/a4syn/err.txt; rc=1
  else
    w=$(grep -c warning: /tmp/a4syn/err.txt)
    echo "ok   $f (warnings: $w)"
    [ "$w" != "0" ] && grep warning: /tmp/a4syn/err.txt | head -8
  fi
done
exit $rc
