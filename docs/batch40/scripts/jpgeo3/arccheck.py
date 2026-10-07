import json, sys, urllib.request, datetime
def get(u, t=90):
    for i in range(3):
        try: return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=t))
        except Exception as e: err=e
    raise err
for L in sys.argv[1:]:
    m = get(L + '?f=json')
    oid = m.get('objectIdField') or next((f['name'] for f in m['fields'] if f['type']=='esriFieldTypeOID'), None)
    ps = min(1000, m.get('maxRecordCount') or 1000)
    cnt = get(L + '/query?where=1%3D1&returnCountOnly=true&f=json').get('count')
    ei = m.get('editingInfo', {}); le = ei.get('dataLastEditDate') or ei.get('lastEditDate')
    q = lambda off: L + f'/query?where=1%3D1&outFields=*&outSR=4326&f=json&orderByFields={oid}&resultRecordCount={ps}&resultOffset={off}'
    p1 = get(q(0))['features']; p2 = get(q(ps))['features']
    o1 = [f['attributes'][oid] for f in p1]; o2 = [f['attributes'][oid] for f in p2]
    print(f"{L.split('/rest/services/')[1]} | oid={oid} maxRC={m.get('maxRecordCount')} ps={ps} count={cnt} page_max={-(-cnt//ps)+2} lastEdit={datetime.datetime.fromtimestamp(le/1000).date() if le else None} geom={m.get('geometryType')} | p1 {len(p1)} [{o1[0] if o1 else None}..{o1[-1] if o1 else None}] p2 {len(p2)} [{o2[0] if o2 else None}..{o2[-1] if o2 else None}] overlap={len(set(o1)&set(o2))}")
