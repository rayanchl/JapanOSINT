#!/bin/bash
python3 - <<'EOF'
import json, urllib.request, time
for seg in ('fac','net'):
    req = urllib.request.Request(
        "https://www.peeringdb.com/api/%s?country=JP&depth=0&limit=1000" % seg,
        headers={'accept': 'application/json', 'User-Agent': 'audit/1.0'})
    d = json.loads(urllib.request.urlopen(req, timeout=60).read())['data']
    ks = set()
    for x in d: ks |= set(x.keys())
    coord = sum(1 for x in d if x.get('latitude') not in (None, '')
                and x.get('longitude') not in (None, ''))
    print(seg, 'n=%d' % len(d), 'with_latlon=%d' % coord)
    print('  keys:', sorted(ks))
    print('  sample:', json.dumps({k: d[0].get(k) for k in
          ('id','name','city','latitude','longitude','updated','created','asn','website')},
          ensure_ascii=False))
    time.sleep(3)
EOF
