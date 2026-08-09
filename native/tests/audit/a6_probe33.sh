#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_typhoon.txt
: > "$OUT"
TC=TC2615
for p in specifications.json forecast.json history.json ; do
  echo "--- $p" >> "$OUT"
  curl -sS -m 20 -o /tmp/a6ty.json -w "[%{http_code}] %{size_download}b\n" \
    "https://www.jma.go.jp/bosai/typhoon/data/$TC/$p" >> "$OUT"
  python3 -c "
import json,sys
try:
    d=json.load(open('/tmp/a6ty.json'))
except Exception as e:
    print('  parse fail', e); sys.exit()
print('  type', type(d).__name__, 'len', len(d))
print('  ', json.dumps(d, ensure_ascii=False)[:1500])
" >> "$OUT"
done
cat "$OUT"
