#!/bin/bash
d="${1:-/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a4_fix}"
n="${2:-12}"
for f in "$d"/*.txt; do
  echo "######## $(basename "$f")"
  head -"$n" "$f"
done
