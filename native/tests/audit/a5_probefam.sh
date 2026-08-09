#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
echo "=== slice files that include lib/probe.h"
while read -r f; do
  [ -z "$f" ] && continue
  if grep -q 'lib/probe.h' "collectors/sources/$f" 2>/dev/null; then
    ids=$(grep -oE '\.id = "[^"]+"' "collectors/sources/$f" | sed 's/.id = //' | tr '\n' ' ')
    echo "  $f  $ids"
  fi
done < tests/audit/a5_files.txt
echo
echo "=== how many sources in the WHOLE tree are probe-only"
grep -l 'lib/probe.h' collectors/sources/*.c | wc -l
echo "=== probe_head call sites tree-wide"
grep -c 'probe_head' collectors/sources/*.c 2>/dev/null | grep -v ':0' | wc -l
