#!/bin/bash
# auditor a4 helper: run a list of sources, dump results to a4_out/<id>.txt
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native
OUT="${OUT:-tests/audit/a4_out}"
mkdir -p "$OUT"
ROWS="${ROWS:-3}"
TMO="${TMO:-180}"
while read -r id ent; do
  [ -z "$id" ] && continue
  f="$OUT/$id.txt"
  if [ -n "$ent" ]; then
    python3 tests/audit/verify_one.py --slot a4 --stderr --rows "$ROWS" --timeout "$TMO" "$id" "$ent" > "$f" 2>&1
  else
    python3 tests/audit/verify_one.py --slot a4 --stderr --rows "$ROWS" --timeout "$TMO" "$id" > "$f" 2>&1
  fi
  echo "== $id: $(grep -m1 '^sched:' "$f") | $(grep -m1 '^--- persisted' "$f")"
done < "$1"
