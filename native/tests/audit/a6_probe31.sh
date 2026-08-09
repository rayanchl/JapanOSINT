#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_portals.txt
: > "$OUT"
for u in "https://www.soumu.go.jp/senkyo/" \
         "https://iss.ndl.go.jp/api/opensearch?title=%E6%97%A5%E6%9C%AC" \
         "https://www.courts.go.jp/app/hanrei_jp/list1" \
         "https://chiebukuro.yahoo.co.jp/" \
         "https://www.nhk.or.jp/corporateinfo/english/operations/" \
         "https://traininfo.jreast.co.jp/delay_certificate/" \
         "https://www.vics.or.jp/" \
         "https://global.jr-central.co.jp/en/info/status/" \
         "https://www.fsa.go.jp/policy/virtual_currency02/" ; do
  curl -sS -o /tmp/a6p.html -w "%{http_code} %{size_download}b %{time_total}s  $u\n" -m 25 -L \
    -A "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/126.0 Safari/537.36" "$u" >> "$OUT" 2>&1
done
echo "=== ndl opensearch items ===" >> "$OUT"
curl -sS -m 25 "https://iss.ndl.go.jp/api/opensearch?title=%E6%97%A5%E6%9C%AC&cnt=5" -o /tmp/a6ndl.xml
echo "size=$(wc -c < /tmp/a6ndl.xml) items=$(grep -o -c '<item>' /tmp/a6ndl.xml)" >> "$OUT"
head -c 500 /tmp/a6ndl.xml >> "$OUT"
cat "$OUT"
