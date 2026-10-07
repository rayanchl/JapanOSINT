#!/bin/bash
# usage: chk.sh <host-or-path-substring>...
for s in "$@"; do
  s2=$(echo "$s" | sed -E 's#^https?://##' | tr 'A-Z' 'a-z')
  echo "== $s2"
  grep -iF "$s2" /Users/rayan/OSINTsaas/docs/batch40/existing_urls.txt | head -15
  grep -iF "$s2" /Users/rayan/OSINTsaas/docs/batch40/jpnat3_scratch/b32_rejres_urls.txt | sed 's/^/  [b32rej] /' | head -10
  # other batch40 beats in progress
  cat /Users/rayan/OSINTsaas/docs/candidate-sources-batch40.*.txt 2>/dev/null | grep -iF "$s2" | cut -d'|' -f1,10 | sed 's/^/  [b33] /' | head -5
done
