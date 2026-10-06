import json, sys, urllib.request, urllib.parse, re, os, glob
from concurrent.futures import ThreadPoolExecutor
D = os.path.dirname(os.path.abspath(__file__))
TW = re.compile(r'[處區臺灣圖鄉鎮與縣資訊學們這個為從]')
orgs = {}
for l in open(os.path.join(D, 'services.jsonl')):
    r = json.loads(l)
    m = re.match(r'(https://services[0-9-a-z]*\.arcgis\.com/[A-Za-z0-9]+)/arcgis/rest/services', r['url'])
    if not m: continue
    t = r.get('title') or ''
    jp = (r.get('culture') or '').startswith('ja') or re.search(r'[぀-ヿ]', t)
    o = m.group(1)
    orgs.setdefault(o, [0, 0, set()])
    if jp: orgs[o][0] += 1
    if TW.search(t): orgs[o][1] += 1
    orgs[o][2].add(r.get('owner'))
cand = [o for o, v in orgs.items() if v[0] >= 1 and v[1] == 0]
print(len(orgs), len(cand), file=sys.stderr)
def get(u, t=30):
    req = urllib.request.Request(u, headers={'User-Agent': 'JapanOSINT/1.0'})
    return json.load(urllib.request.urlopen(req, timeout=t))
def work(o):
    try:
        d = get(o + '/arcgis/rest/services?f=json')
        return o, [s['url'] if 'url' in s else f"{o}/arcgis/rest/services/{s['name']}/{s['type']}" for s in d.get('services', [])]
    except Exception as e:
        return o, 'ERR ' + str(e)[:60]
out = {}
with ThreadPoolExecutor(12) as ex:
    for o, s in ex.map(work, cand):
        out[o] = s
json.dump(out, open(os.path.join(D, 'orgdir.json'), 'w'))
print(sum(len(v) for v in out.values() if isinstance(v, list)), file=sys.stderr)
