#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_nra2.txt
: > "$OUT"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
curl -sS -L -m 25 -A "$B" https://radioactivity.nra.go.jp/main.min.js -o /tmp/a6mj.js
echo "main.min.js size=$(wc -c < /tmp/a6mj.js)" >> "$OUT"
grep -o -E "[\"'][^\"']*(\.json|/api/|/cont/)[^\"']*[\"']" /tmp/a6mj.js | sort -u | head -40 >> "$OUT"
curl -sS -L -m 25 -A "$B" https://radioactivity.nra.go.jp/_nuxt/Dmulr-4x.js -o /tmp/a6nj.js
echo "nuxt size=$(wc -c < /tmp/a6nj.js)" >> "$OUT"
grep -o -E "[\"'][^\"']*(\.json|/api/|/cont/)[^\"']*[\"']" /tmp/a6nj.js | sort -u | head -40 >> "$OUT"
echo "=== _nuxt dir listing of index html script tags ===" >> "$OUT"
grep -o -E '/_nuxt/[A-Za-z0-9_.-]+\.js' /tmp/a6nr.html 2>/dev/null | sort -u | head >> "$OUT"
cat "$OUT"
