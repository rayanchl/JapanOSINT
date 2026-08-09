#!/bin/bash
echo "=== tide_area.json head"
curl -s --max-time 25 "https://www.jma.go.jp/bosai/tidelevel/const/tide_area.json" -o /tmp/ta.json
python3 - <<'EOF'
import json
d = json.load(open('/tmp/ta.json'))
k = list(d.keys())[0]
print('class20 key:', k)
a = d[k]
print('area keys:', list(a.keys()))
c30 = a['class30s'][0]
print('class30 keys:', list(c30.keys()), 'code=', c30.get('code'), 'name=', c30.get('name'))
st = c30['stations'][0]
print('station:', json.dumps(st, ensure_ascii=False))
EOF
echo
echo "=== bosai tide page fragment hints"
curl -s --max-time 25 "https://www.jma.go.jp/bosai/tide/" -o /tmp/tp.html
grep -o 'area_type[^"<> ]*' /tmp/tp.html | head -5
grep -o 'stationCode[^"<> ]*' /tmp/tp.html | head -5
echo "=== peeringdb sample"
curl -s --max-time 30 "https://www.peeringdb.com/api/ix?country=JP&limit=2" -o /tmp/pdb.json
head -c 1200 /tmp/pdb.json
