#!/bin/bash
for seg in ix fac net; do
  curl -s --max-time 40 -H 'accept: application/json' \
    "https://www.peeringdb.com/api/$seg?country=JP&depth=0&limit=1000" -o /tmp/pdb_$seg.json
done
python3 - <<'EOF'
import json
for seg in ('ix','fac','net'):
    d = json.load(open('/tmp/pdb_%s.json' % seg))['data']
    ks = set()
    for x in d: ks |= set(x.keys())
    coord = sum(1 for x in d if x.get('latitude') not in (None,'') and x.get('longitude') not in (None,''))
    print(seg, 'n=%d' % len(d), 'with_latlon=%d' % coord)
    print('  keys:', sorted(ks))
EOF
