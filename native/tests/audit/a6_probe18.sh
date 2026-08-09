#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jaxa7.txt
: > "$OUT"
curl -sS -L -m 25 "https://data.earth.jaxa.jp/stac/cog/v1/catalog.json" -o /tmp/a6cat.json
echo "size=$(wc -c < /tmp/a6cat.json)" >> "$OUT"
python3 - <<'PY' >> "$OUT"
import json
d=json.load(open('/tmp/a6cat.json'))
print("keys:", list(d.keys()))
for k,v in d.items():
    if k!="links": print(" ",k,"=",json.dumps(v,ensure_ascii=False)[:200])
ls=d.get("links",[])
print("links:",len(ls))
from collections import Counter
print(Counter(l.get("rel") for l in ls))
for l in ls[:6]: print("  ",json.dumps(l,ensure_ascii=False)[:220])
PY
child=$(python3 -c "import json;d=json.load(open('/tmp/a6cat.json'));print([l['href'] for l in d['links'] if l.get('rel')=='child'][0])")
echo "=== first child: $child" >> "$OUT"
curl -sS -L -m 25 "$child" -o /tmp/a6col.json
python3 - <<'PY' >> "$OUT"
import json
d=json.load(open('/tmp/a6col.json'))
for k,v in d.items():
    print(" ",k,"=",json.dumps(v,ensure_ascii=False)[:260])
PY
cat "$OUT"
