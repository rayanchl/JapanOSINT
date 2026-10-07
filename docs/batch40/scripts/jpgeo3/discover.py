import json, sys, urllib.request, urllib.parse, time, re, os
from concurrent.futures import ThreadPoolExecutor
OUT = os.path.join(os.path.dirname(__file__), 'services.jsonl')
kws = sys.argv[1:]
seen = set()
if os.path.exists(OUT):
    for l in open(OUT):
        seen.add(json.loads(l)['url'])
def get(u):
    req = urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'})
    return json.load(urllib.request.urlopen(req, timeout=40))
def search(kw):
    res = []
    start = 1
    while start > 0 and start <= 1000:
        q = f'{kw} AND type:"Feature Service" AND access:public'
        u = 'https://www.arcgis.com/sharing/rest/search?' + urllib.parse.urlencode({'q': q, 'num': 100, 'start': start, 'f': 'json'})
        try:
            d = get(u)
        except Exception as e:
            print('ERR', kw, e, file=sys.stderr); break
        for r in d.get('results', []):
            res.append(r)
        start = d.get('nextStart', -1)
    return kw, res
with open(OUT, 'a') as f, ThreadPoolExecutor(6) as ex:
    for kw, res in ex.map(search, kws):
        n = 0
        for r in res:
            u = r.get('url') or ''
            if not u or u in seen: continue
            seen.add(u); n += 1
            f.write(json.dumps({'kw': kw, 'url': u, 'title': r.get('title'), 'owner': r.get('owner'), 'orgId': r.get('orgId'), 'culture': r.get('culture'), 'modified': r.get('modified'), 'id': r.get('id'), 'numViews': r.get('numViews')}, ensure_ascii=False) + '\n')
        print(kw, len(res), 'new', n, file=sys.stderr)
