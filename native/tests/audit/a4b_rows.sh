#!/bin/bash
d=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit
for f in "$@"; do
  for dir in a4b_out3 a4b_out2 a4b_out1 a4_fix a4_out2 a4_out; do
    [ -f "$d/$dir/$f.txt" ] && { echo "######## $f  ($dir)"; \
      sed -n '/--- row 1 ---/,$p' "$d/$dir/$f.txt" | head -13; break; }
  done
done
