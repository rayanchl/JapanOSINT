import json, sys, urllib.request, collections
def get(u, t=90):
    for i in range(3):
        try: return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=t))
        except Exception as e: err=e
    raise err
host, rid = sys.argv[1], sys.argv[2]
keys = sys.argv[3].split(',') if len(sys.argv) > 3 else []
recs=[]; off=0; pages=0
while True:
    r = get(f'https://{host}/api/3/action/datastore_search?resource_id={rid}&limit=1000&offset={off}&sort=_id')['result']
    pages+=1
    recs += r['records']
    if len(r['records']) < 1000: break
    off += 1000
print('total', r.get('total'), 'fetched', len(recs), 'pages', pages)
for k in keys:
    ks = k.split('+')
    c = collections.Counter(tuple(str(x.get(z)) for z in ks) for x in recs)
    print(f'  {k}: distinct {len(c)} top {c.most_common(2)}'[:300])
print('first', json.dumps(recs[0], ensure_ascii=False)[:300])
print('last', json.dumps(recs[-1], ensure_ascii=False)[:300])
