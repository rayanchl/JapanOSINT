import json, time, urllib.request, sys
sys.path.insert(0,'/Users/rayan/OSINTsaas/native/tools')
from probe_hp_batch import jsonp_inner
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
recs=[]
for p in range(0,320):
    body="page=%d&typejb=T&xxfcbj%%5B%%5D=0&xxfcbj%%5B%%5D=1&xxzqdm=&sortfield=xxzqdm&sorttype=asc"%p
    req=urllib.request.Request("https://www.neeq.com.cn/nqxxController/nqxxCnzq.do",data=body.encode(),headers={"User-Agent":UA,"Content-Type":"application/x-www-form-urlencoded"},method="POST")
    d=json.loads(jsonp_inner(urllib.request.urlopen(req,timeout=60).read().decode('utf-8')))
    c=d[0].get('content')
    if c is None: print('no content',d,file=sys.stderr); break
    if not c: print('empty at page',p, {k:v for k,v in d[0].items() if k!='content'}); break
    recs+=c
    time.sleep(1.0)
json.dump(recs,open('neeq_sec_all.json','w'),ensure_ascii=False)
print(len(recs), len(set(x['xxzqdm'] for x in recs)))
