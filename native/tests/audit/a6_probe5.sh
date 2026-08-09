#!/usr/bin/env bash
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
echo "=== maritime articles.rss body ==="
curl -sS -o /tmp/a6m.xml -w '%{http_code} %{size_download}\n' -m 15 -H "Accept: application/rss+xml,*/*" -A "japanosint-collector" https://maritime-executive.com/articles.rss
head -c 400 /tmp/a6m.xml; echo
grep -c -i '<item' /tmp/a6m.xml
echo "=== rferl feed labels ==="
curl -sS -L -m 20 -A "$B" https://www.rferl.org/rssfeeds > /tmp/a6r.html
python3 - <<'PY'
import re
h=open('/tmp/a6r.html',encoding='utf-8',errors='replace').read()
for m in re.finditer(r'href="(/api/[^"]+)"[^>]*>(.{0,120}?)</a>', h, re.S):
    t=re.sub(r'<[^>]+>','',m.group(2)).strip()
    if t: print(m.group(1), '|', t[:60])
PY
