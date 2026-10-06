import json,urllib.request,sys
from concurrent.futures import ThreadPoolExecutor
def get(u): return json.load(urllib.request.urlopen(urllib.request.Request(u,headers={'User-Agent':'JapanOSINT/1.0'}),timeout=60))
ex=open('/Users/rayan/OSINTsaas/docs/batch38/existing_urls.txt').read().lower()
base=sys.argv[1]
d=get(base+'/arcgis/rest/services?f=json')
svcs=[s for s in d['services'] if s['type']=='FeatureServer']
def w(s):
    u=f"{base}/arcgis/rest/services/{s['name']}/FeatureServer"
    try:
        m=get(u+'?f=json')
        out=[]
        for L in ((m.get('layers') or [])+(m.get('tables') or []))[:10]:
            try: c=get(f"{u}/{L['id']}/query?where=1%3D1&returnCountOnly=true&f=json").get('count')
            except Exception: c=None
            out.append((c or 0, s['name'], L['id'], L['name'], (L.get('geometryType') or 'table')[12:], 'EXIST' if u.lower().split('://')[1] in ex else ''))
        return out
    except Exception as e:
        return [(0, s['name'], str(e)[:40])]
with ThreadPoolExecutor(12) as e:
    res=[x for r in e.map(w, svcs) for x in r]
res.sort(key=lambda x:-x[0])
print(len(svcs),'services')
for r in res[:int(sys.argv[2]) if len(sys.argv)>2 else 40]: print(r)
