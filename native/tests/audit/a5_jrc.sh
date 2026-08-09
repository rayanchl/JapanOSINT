#!/usr/bin/env bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/126.0 Safari/537.36'
curl -sSL -m 25 -A "$UA" https://traininfo.jr-central.co.jp/zairaisen/index.html -o /tmp/jrc.html
echo "bytes=$(wc -c </tmp/jrc.html)"
echo "--- json-ish urls referenced ---"
grep -oiE "['\"][^'\"]{0,120}\.json[^'\"]{0,40}['\"]" /tmp/jrc.html | sort -u | head -20
echo "--- api paths ---"
grep -oiE "['\"]/?[a-z0-9_/.-]*(api|data|json)[a-z0-9_/.-]*['\"]" /tmp/jrc.html | sort -u | head -20
echo "--- try known endpoints ---"
for u in \
  https://traininfo.jr-central.co.jp/zairaisen/data/train_info.json \
  https://traininfo.jr-central.co.jp/api/zairaisen \
  https://traininfo.jr-central.co.jp/zairaisen/json/status.json ; do
  printf '%-62s ' "$u"
  curl -sSL -m 15 -A "$UA" -o /dev/null -w '%{http_code} %{content_type}\n' "$u"
done
