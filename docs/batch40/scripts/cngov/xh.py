import re,json,urllib.request,collections,sys,time
UA={'User-Agent':'JapanOSINT/1.0'}
def get(u):
    return urllib.request.urlopen(urllib.request.Request(u,headers=UA),timeout=40).read().decode('utf-8','replace')
for ch in sys.argv[1:]:
    try: s=get(f'https://www.news.cn/{ch}/')
    except Exception as e: print(ch,'ERR',e); continue
    ids=list(dict.fromkeys(re.findall(r'datasource:([0-9a-f]{32})',s)))
    best=[]
    for i in ids:
        try: d=json.loads(get(f'https://www.news.cn/{ch}/ds_{i}.json'))
        except Exception as e: continue
        ds=d.get('datasource') or []
        if len(ds)<50: continue
        t=sorted((x.get('publishTime') or '') for x in ds)
        c=collections.Counter(x[:10] for x in t)
        last7=[c[k] for k in sorted(c)[-8:-1]]
        best.append((len(ds),i,d.get('categoryName'),t[0][:10],t[-1][:16],last7,len(set(x['contentId'] for x in ds))))
        time.sleep(0.3)
    for b in sorted(best,reverse=True)[:4]: print(ch,b)
