#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_gvp.txt
curl -sS -m 25 -A 'JapanOSINT/1.0 (hazard-feed)' "https://volcano.si.edu/news/WeeklyVolcanoRSS.xml" -o /tmp/a6gvp.xml
{
  echo "size=$(wc -c < /tmp/a6gvp.xml) items=$(grep -o -c '<item>' /tmp/a6gvp.xml)"
  echo "=== links ==="
  grep -o '<link>[^<]*' /tmp/a6gvp.xml | head -8
  echo "=== first two items ==="
  python3 - <<'PY'
import re
x=open('/tmp/a6gvp.xml',encoding='utf-8',errors='replace').read()
its=re.findall(r'<item>(.*?)</item>', x, re.S)
print("item count:", len(its))
for it in its[:2]:
    print("-----")
    print(it[:1500])
PY
} > "$OUT"
cat "$OUT"
