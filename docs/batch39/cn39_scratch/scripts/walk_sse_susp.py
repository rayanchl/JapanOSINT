import json, time, urllib.request, collections, sys
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
base="https://query.sse.com.cn/commonSoaQuery.do?isPagination=true&sqlId=GW_PL_JYTS_TFPXX&pageHelp.pageSize=1000&pageHelp.pageNo=%d&productCode=&keyWords=&startStopDate=19901219&endStopDate="
allr=[]
for p in range(1,32):
    req=urllib.request.Request(base%p,headers={"User-Agent":UA,"Referer":"https://www.sse.com.cn/"})
    d=json.load(urllib.request.urlopen(req,timeout=60))
    data=d['pageHelp']['data'] or []
    print(p,len(data),d['pageHelp']['total'],file=sys.stderr)
    if not data: break
    allr+=data
    time.sleep(2)
json.dump(allr,open('sse_susp_all.json','w'),ensure_ascii=False)
print(len(allr))
for keys in [('productCode','startStopDate'),('productCode','startStopDate','type'),('productCode','startStopDate','type','stopTime'),('productCode','startStopDate','type','stopTime','endStopDate','stopReason','controlType')]:
    print(keys, len(set(tuple(r.get(k) for k in keys) for r in allr)))
print('full distinct', len(set(json.dumps(r,sort_keys=True,ensure_ascii=False) for r in allr)))
print(collections.Counter(r['type'] for r in allr))
print(min(r['startStopDate'] for r in allr), max(r['startStopDate'] for r in allr))
print(json.dumps(allr[0],ensure_ascii=False)); print(json.dumps(allr[-1],ensure_ascii=False))
