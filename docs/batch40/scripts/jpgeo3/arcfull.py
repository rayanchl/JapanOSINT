import json, sys, urllib.request, collections, datetime
def get(u, t=90):
    for i in range(3):
        try:
            return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=t))
        except Exception as e:
            err = e
    raise err
L = sys.argv[1].rstrip('/')
keys = sys.argv[2].split(',') if len(sys.argv) > 2 else []
m = get(L + '?f=json')
oid = m.get('objectIdField') or next((f['name'] for f in m['fields'] if f['type'] == 'esriFieldTypeOID'), 'OBJECTID')
ps = min(1000, m.get('maxRecordCount') or 1000)
cnt = get(L + '/query?where=1%3D1&returnCountOnly=true&f=json').get('count')
le = m.get('editingInfo', {}).get('lastEditDate') or m.get('editingInfo', {}).get('dataLastEditDate')
print('layer', m.get('name'), '| geom', m.get('geometryType'), '| maxRC', m.get('maxRecordCount'), '| oid', oid, '| count', cnt, '| lastEdit', datetime.datetime.utcfromtimestamp(le/1000).date() if le else None)
feats = []; off = 0; pages = []
while True:
    dd = get(L + f'/query?where=1%3D1&outFields=*&outSR=4326&f=json&resultRecordCount={ps}&resultOffset={off}')
    fs = dd.get('features', [])
    if 'error' in dd: print('ERROR', dd['error']); break
    pages.append(fs[0]['attributes'].get(oid) if fs else None)
    feats += fs
    if not fs: break
    off += ps
print('fetched', len(feats), 'pages', len(pages), 'first-oid per page', pages[:3])
ids = collections.Counter(f['attributes'].get(oid) for f in feats)
print('distinct', oid, len(ids))
for k in keys:
    if '+' in k:
        ks = k.split('+'); c = collections.Counter(tuple(str(f['attributes'].get(x)) for x in ks) for f in feats)
    else:
        c = collections.Counter(str(f['attributes'].get(k)) for f in feats)
    print(f'  {k}: distinct {len(c)} nulls {c.get("None",0)} top {[x for x in c.most_common(2)]}'[:300])
g = [f.get('geometry') for f in feats[:3]]
print('geom sample', str(g)[:200])
print('sample', json.dumps(feats[0]['attributes'], ensure_ascii=False)[:700] if feats else None)
