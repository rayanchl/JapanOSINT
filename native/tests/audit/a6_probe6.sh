#!/usr/bin/env bash
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
curl -sS -L -m 20 -A "$B" https://www.rferl.org/rssfeeds > /tmp/a6r.html
python3 - <<'PY' > /tmp/a6r.txt
import re
h=open('/tmp/a6r.html',encoding='utf-8',errors='replace').read()
ids=[]
for m in re.finditer(r'href="(/api/[^"]+)"', h):
    if m.group(1) not in ids: ids.append(m.group(1))
print("\n".join(ids[:24]))
PY
while read -r p; do
  [ -z "$p" ] && continue
  curl -sS -o /tmp/a6f2.xml -m 12 -A "japanosint-collector" "https://www.rferl.org$p"
  t=$(grep -o -i '<title>[^<]*' /tmp/a6f2.xml | head -1 | sed 's/<title>//')
  n=$(grep -c -i '<item>' /tmp/a6f2.xml)
  printf '%-34s items=%-4s %s\n' "$p" "$n" "$t"
done < /tmp/a6r.txt
echo "=== maritime entries ==="
curl -sS -o /tmp/a6m.xml -m 15 -A "japanosint-collector" https://maritime-executive.com/articles.rss
echo "entries=$(grep -c -i '<entry' /tmp/a6m.xml)"
grep -o -i '<title>[^<]*' /tmp/a6m.xml | head -4
