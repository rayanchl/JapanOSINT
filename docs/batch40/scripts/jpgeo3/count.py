import json, sys, urllib.request, urllib.parse, re, os, glob
from concurrent.futures import ThreadPoolExecutor
D = os.path.dirname(os.path.abspath(__file__))
ex = open(os.path.join(D, '../existing_urls.txt')).read().lower()
rej = ''
for f in glob.glob(os.path.join(D, '../../rejected-sources-batch32.*.tsv')) + [os.path.join(D, '../../reserve-sources-batch32.tsv')] + glob.glob(os.path.join(D, '../../candidate-sources-batch32.*.txt')):
    rej += open(f).read().lower()
TW = re.compile(r'[處區臺灣圖鄉鎮與縣資訊學們這個為從]')
done = set()
OUT = os.path.join(D, 'layers.jsonl')
if os.path.exists(OUT):
    for l in open(OUT):
        done.add(json.loads(l)['svc'])
def get(u, t=30):
    req = urllib.request.Request(u, headers={'User-Agent': 'JapanOSINT/1.0'})
    return json.load(urllib.request.urlopen(req, timeout=t))
svcs = {}
for l in open(os.path.join(D, 'services.jsonl')):
    r = json.loads(l)
    u = r['url'].rstrip('/')
    t = r.get('title') or ''
    jp = (r.get('culture') or '').startswith('ja') or re.search(r'[぀-ヿ]', t)
    if not jp or TW.search(t): continue
    if re.search(r'_form$|_fieldworker$|_results$|アンケート|フォーム|デモ|demo|sample|サンプル|test|テスト|演習|課題|練習', t, re.I): continue
    m = re.match(r'(https?://[^?]+?/FeatureServer)(/\d+)?$', u)
    if not m: continue
    s = m.group(1)
    k = re.sub(r'^https?://', '', s).lower()
    if k in ex or urllib.parse.unquote(k) in ex or k in rej or urllib.parse.unquote(k) in rej: continue
    svcs[s] = r
todo = [s for s in svcs if s not in done]
print(len(svcs), 'todo', len(todo), file=sys.stderr)
def work(s):
    out = []
    try:
        d = get(s + '?f=json')
    except Exception as e:
        return [{'svc': s, 'err': str(e)[:80]}]
    if 'error' in d:
        return [{'svc': s, 'err': str(d['error'].get('code'))}]
    lyrs = (d.get('layers') or []) + (d.get('tables') or [])
    for L in lyrs[:12]:
        lu = f"{s}/{L['id']}"
        try:
            c = get(lu + '/query?where=1%3D1&returnCountOnly=true&f=json').get('count')
        except Exception as e:
            c = None
        out.append({'svc': s, 'layer': L['id'], 'lname': L.get('name'), 'count': c, 'geom': L.get('geometryType'), 'title': svcs[s].get('title'), 'owner': svcs[s].get('owner'), 'modified': svcs[s].get('modified'), 'maxrc': d.get('maxRecordCount')})
    if not out: out = [{'svc': s, 'err': 'nolayers'}]
    return out
with open(OUT, 'a') as f, ThreadPoolExecutor(16) as ex_:
    for i, res in enumerate(ex_.map(work, todo)):
        for r in res: f.write(json.dumps(r, ensure_ascii=False) + '\n')
        if i % 200 == 0: print(i, file=sys.stderr); f.flush()
