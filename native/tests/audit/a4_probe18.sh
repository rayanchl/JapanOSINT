#!/bin/bash
curl -sL --max-time 30 'https://www.jma.go.jp/bosai/quake/data/list.json' -o /tmp/jma.json -w '%{http_code} %{size_download}\n'
python3 - <<'EOF'
import json, collections
d = json.load(open('/tmp/jma.json'))
print("entries:", len(d))
print("unique eid:", len({x.get('eid') for x in d}))
keys = collections.Counter()
for x in d: keys.update(x.keys())
print("keys:", dict(keys))
for x in d[:3]: print(json.dumps(x, ensure_ascii=False))
EOF
