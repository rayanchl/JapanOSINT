#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_nra.txt
: > "$OUT"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
curl -sS -L -m 25 -A "$B" https://radioactivity.nra.go.jp/ -o /tmp/a6nr.html
echo "size=$(wc -c < /tmp/a6nr.html)" >> "$OUT"
grep -o -E 'src="[^"]+"' /tmp/a6nr.html | sort -u | head -25 >> "$OUT"
grep -o -E 'href="[^"]+"' /tmp/a6nr.html | grep -i -E 'json|csv|api|map' | sort -u | head -25 >> "$OUT"
echo "=== ja map page ===" >> "$OUT"
for u in https://radioactivity.nra.go.jp/map/ https://radioactivity.nra.go.jp/ja/ https://radioactivity.nra.go.jp/cont/ ; do
  c=$(curl -sS -o /tmp/a6m2.html -w '%{http_code}' -L -m 20 -A "$B" "$u")
  echo "$u [$c] $(wc -c < /tmp/a6m2.html)" >> "$OUT"
done
cat "$OUT"
