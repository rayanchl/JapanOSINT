import json, sys, urllib.request, collections
def get(u, t=60):
    return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=t))
host, rid = sys.argv[1], sys.argv[2]
full = len(sys.argv) > 3
d = get(f'https://{host}/api/3/action/datastore_search?resource_id={rid}&limit=3&offset=0&sort=_id')
r = d['result']
print('total', r.get('total'), 'fields', [f['id'] for f in r['fields']])
for rec in r['records'][:3]:
    print(json.dumps(rec, ensure_ascii=False)[:600])
if full:
    recs = []
    off = 0
    while True:
        dd = get(f'https://{host}/api/3/action/datastore_search?resource_id={rid}&limit=1000&offset={off}&sort=_id')
        rr = dd['result']['records']
        recs += rr
        if len(rr) < 1000: break
        off += 1000
    print('fetched', len(recs))
    for k in r['fields']:
        vals = [str(x.get(k['id'])) for x in recs]
        c = collections.Counter(vals)
        print(f"  {k['id']}: distinct {len(c)} top {c.most_common(1)[0][:2]}")
    print('last rec', json.dumps(recs[-1], ensure_ascii=False)[:500])
