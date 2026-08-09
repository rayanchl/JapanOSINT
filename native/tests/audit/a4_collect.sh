#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit
{ for d in a4_out a4_out2; do
    for f in $d/*.txt; do
      id=$(basename "$f" .txt)
      sc=$(grep -m1 '^sched:' "$f")
      pe=$(grep -m1 '^--- persisted' "$f" | sed 's/--- persisted: //; s/ ---//')
      rt=$(grep -m1 '^record_type:' "$f" | sed 's/record_type: //')
      echo -e "$id\t$sc\t$pe\t$rt"
    done
  done; } | sort -u > a4_results.tsv
echo "rows: $(wc -l < a4_results.tsv)"
echo "=== nonzero rc:"
grep -P 'rc=(?!0 )' a4_results.tsv | head -40
