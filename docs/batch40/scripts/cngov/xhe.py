import re,json,urllib.request,collections,sys,time
UA={'User-Agent':'JapanOSINT/1.0'}
def get(u): return urllib.request.urlopen(urllib.request.Request(u,headers=UA),timeout=40).read().decode('utf-8','replace')
for page in sys.argv[1:]:
    s=get(page); base=page.rsplit('/',1)[0]
    ids=list(dict.fromkeys(re.findall(r'datasource:([0-9a-f]{32})',s)))
    for i in ids:
        try: d=json.loads(get(f'{base}/ds_{i}.json'))
        except Exception as e: continue
        ds=d.get('datasource') or []
        if len(ds)<50: continue
        t=sorted((x.get('publishTime') or '') for x in ds)
        c=collections.Counter(x[:10] for x in t)
        print(page.split('/')[-2], len(ds), i, d.get('categoryName'), t[0][:10], t[-1][:16], [c[k] for k in sorted(c)[-8:-1]], len(set(x['contentId'] for x in ds)))
        time.sleep(0.3)
