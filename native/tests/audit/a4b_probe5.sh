#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
echo "=== BoM warnings v1"
curl -s --max-time 30 -A "$UA" "https://api.weather.bom.gov.au/v1/warnings" -o /tmp/bom.json
python3 - <<'EOF'
import json
d = json.load(open('/tmp/bom.json'))
print('top keys', list(d.keys()))
data = d.get('data', [])
print('n', len(data))
print(json.dumps(data[0], indent=1))
print('--- metadata:', json.dumps(d.get('metadata'), indent=1))
ks = set()
for x in data: ks |= set(x.keys())
print('all item keys:', sorted(ks))
EOF
echo
echo "=== jma tide sample"
curl -s --max-time 30 "https://www.jma.go.jp/bosai/tide/data/list.json" | head -c 600
echo
