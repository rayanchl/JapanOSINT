import json, sys, urllib.request, os
from concurrent.futures import ThreadPoolExecutor
D = os.path.dirname(os.path.abspath(__file__))
todo = json.load(open(os.path.join(D, 'todo2.json')))
OUT = os.path.join(D, 'layers2.jsonl')
done = set()
if os.path.exists(OUT):
    for l in open(OUT): done.add(json.loads(l)['svc'])
todo = [s for s in todo if s not in done]
def get(u, t=25):
    req = urllib.request.Request(u, headers={'User-Agent': 'JapanOSINT/1.0'})
    return json.load(urllib.request.urlopen(req, timeout=t))
def work(s):
    out = []
    try:
        d = get(s + '?f=json')
    except Exception as e:
        return [{'svc': s, 'err': str(e)[:80]}]
    if 'error' in d:
        return [{'svc': s, 'err': str(d['error'].get('code'))}]
    lyrs = (d.get('layers') or []) + (d.get('tables') or [])
    for L in lyrs[:8]:
        if L.get('geometryType') == 'esriGeometryPolygon' or L.get('geometryType') == 'esriGeometryPolyline':
            out.append({'svc': s, 'layer': L['id'], 'lname': L.get('name'), 'count': None, 'geom': L.get('geometryType'), 'maxrc': d.get('maxRecordCount')}); continue
        try:
            c = get(f"{s}/{L['id']}/query?where=1%3D1&returnCountOnly=true&f=json").get('count')
        except Exception:
            c = None
        out.append({'svc': s, 'layer': L['id'], 'lname': L.get('name'), 'count': c, 'geom': L.get('geometryType'), 'maxrc': d.get('maxRecordCount'), 'desc': (d.get('serviceDescription') or '')[:100]})
    if not out: out = [{'svc': s, 'err': 'nolayers'}]
    return out
with open(OUT, 'a') as f, ThreadPoolExecutor(20) as ex_:
    for i, res in enumerate(ex_.map(work, todo)):
        for r in res: f.write(json.dumps(r, ensure_ascii=False) + '\n')
        if i % 500 == 0: print(i, file=sys.stderr); f.flush()
