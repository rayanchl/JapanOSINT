import json, sys, urllib.request, urllib.parse, collections
def get(u, t=60):
    return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=t))
L = sys.argv[1].rstrip('/')
full = len(sys.argv) > 2 and sys.argv[2] == 'full'
m = get(L + '?f=json')
print('name', m.get('name'), 'geom', m.get('geometryType'), 'maxRC', m.get('maxRecordCount'), 'lastEdit', m.get('editingInfo', {}).get('lastEditDate'), 'oid', m.get('objectIdField'))
print('fields', [f['name'] for f in m.get('fields', [])][:60])
cnt = get(L + '/query?where=1%3D1&returnCountOnly=true&f=json').get('count')
print('count', cnt)
q = L + '/query?where=1%3D1&outFields=*&outSR=4326&f=json&resultRecordCount=3&resultOffset=0'
d = get(q)
for f in d.get('features', [])[:2]:
    print(json.dumps(f, ensure_ascii=False)[:900])
if full:
    ps = min(1000, m.get('maxRecordCount') or 1000)
    oid = m.get('objectIdField') or 'OBJECTID'
    feats = []; off = 0
    while True:
        dd = get(L + f'/query?where=1%3D1&outFields=*&outSR=4326&f=json&orderByFields={oid}&resultRecordCount={ps}&resultOffset={off}')
        fs = dd.get('features', [])
        feats += fs
        if not fs or (len(fs) < ps and not dd.get('exceededTransferLimit')): break
        off += ps
    print('fetched', len(feats), 'pagesize', ps)
    ids = collections.Counter(f['attributes'].get(oid) for f in feats)
    print('distinct oid', len(ids))
    for k in list(feats[0]['attributes'].keys())[:40]:
        c = collections.Counter(str(f['attributes'].get(k)) for f in feats)
        print(f'  {k}: distinct {len(c)} e.g. {str(c.most_common(1)[0][0])[:40]}')
