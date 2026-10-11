import json, time, urllib.request, sys
sys.path.insert(0,'/Users/rayan/OSINTsaas/native/tools')
from probe_hp_batch import jsonp_inner
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
def post(url, body):
    req=urllib.request.Request(url,data=body.encode(),headers={"User-Agent":UA,"Content-Type":"application/x-www-form-urlencoded"},method="POST")
    return json.loads(jsonp_inner(urllib.request.urlopen(req,timeout=60).read().decode('utf-8')))
which=sys.argv[1]
recs=[]
for p in range(0,400):
    if which=='sec':
        d=post("https://www.bse.cn/nqxxController/nqxxCnzq.do","page=%d&typejb=T&xxfcbj%%5B%%5D=2&xxzqdm=&sortfield=xxzqdm&sorttype=asc"%p)
        blk=d[0]; c=blk['content']
        meta={k:v for k,v in blk.items() if k!='content'}
    else:
        d=post("https://www.bse.cn/projectNewsController/infoResult.do","page=%d&companyCode=&isNewThree=1&sortfield=updateDate&sorttype=desc&startTime=&endTime=&keyword="%p)
        blk=d[0]['listInfo']; c=blk['content']
        meta={k:v for k,v in blk.items() if k!='content'}
    if p==0: print(meta)
    if not c: print('empty at page',p); break
    recs+=c
    time.sleep(1.5)
json.dump(recs,open('bse_%s_all.json'%which,'w'),ensure_ascii=False)
print(len(recs))
