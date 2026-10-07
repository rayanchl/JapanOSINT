import json, urllib.request, os
D = os.path.dirname(os.path.abspath(__file__))
ex = open(os.path.join(D, '../existing_urls.txt')).read().lower()
def get(u):
    return json.load(urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent':'JapanOSINT/1.0'}), timeout=60))
start=0; out=[]
while True:
    d=get(f'https://www.geospatial.jp/ckan/api/3/action/package_search?rows=1000&start={start}')
    res=d['result']['results']
    for p in res:
        for r in p.get('resources',[]):
            fmt=(r.get('format') or '').lower()
            if fmt in ('csv','geojson','json','xlsx','xml'):
                u=r.get('url') or ''
                k=u.split('://',1)[-1].lower()
                out.append({'org':(p.get('organization') or {}).get('title'),'ds':p.get('title'),'name':r.get('name'),'fmt':fmt,'size':r.get('size'),'url':u,'mod':r.get('last_modified') or r.get('metadata_modified'),'exists':k in ex})
    start+=1000
    print(start, d['result']['count'])
    if start>=d['result']['count'] or not res: break
json.dump(out, open(os.path.join(D,'gsp.json'),'w'), ensure_ascii=False)
print(len(out))
