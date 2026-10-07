import json, sys, urllib.request, urllib.parse, os
from concurrent.futures import ThreadPoolExecutor
D = os.path.dirname(os.path.abspath(__file__))
ex = open(os.path.join(D, '../existing_urls.txt')).read().lower()
def get(u, t=40):
    req = urllib.request.Request(u, headers={'User-Agent': 'JapanOSINT/1.0'})
    return json.load(urllib.request.urlopen(req, timeout=t))
host = sys.argv[1]
res = []
start = 0
while True:
    try:
        d = get(f'https://{host}/api/3/action/package_search?rows=500&start={start}')
    except Exception as e:
        print('ERR', host, e, file=sys.stderr); break
    pk = d['result']['results']
    for p in pk:
        for r in p.get('resources', []):
            if r.get('datastore_active'):
                res.append((p.get('title'), p.get('name'), r.get('id'), r.get('name'), r.get('last_modified') or r.get('metadata_modified'), (p.get('organization') or {}).get('title')))
    start += 500
    if start >= d['result']['count'] or not pk: break
def cnt(x):
    try:
        d = get(f'https://{host}/api/3/action/datastore_search?resource_id={x[2]}&limit=1')
        return x + (d['result'].get('total'), [f['id'] for f in d['result']['fields']][:12])
    except Exception as e:
        return x + (None, str(e)[:50])
with ThreadPoolExecutor(8) as e:
    out = list(e.map(cnt, res))
out.sort(key=lambda x: -(x[6] or 0))
with open(os.path.join(D, f'ckan_{host}.tsv'), 'w') as f:
    for o in out:
        isex = o[2].lower() in ex
        f.write('\t'.join(map(str, (o[6], 'EXIST' if isex else 'new', host, o[2], o[0], o[3], o[4], o[5], o[7]))) + '\n')
print(host, 'datastore resources', len(res), file=sys.stderr)
