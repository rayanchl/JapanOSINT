import json, time, urllib.request, sys
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
codes=sys.argv[1]; maxp=int(sys.argv[2])
recs=[]
for p in range(1,maxp+1):
    body='{"searchRange":1,"sxrq":[],"gbrq":[],"searchType":2,"sxx":[],"gbrqYear":[],"flfgCodeId":[%s],"zdjgCodeId":[],"searchContent":"","orderByParam":{"order":"-1","sort":""},"pageNum":%d,"pageSize":10000}'%(codes,p)
    req=urllib.request.Request("https://flk.npc.gov.cn/law-search/search/list",data=body.encode(),headers={"User-Agent":UA,"Content-Type":"application/json"},method="POST")
    d=json.load(urllib.request.urlopen(req,timeout=120))
    r=d.get('rows')
    print(p, d.get('code'), d.get('total'), None if r is None else len(r), file=sys.stderr)
    if not r: break
    recs+=r
    time.sleep(2)
json.dump(recs,open('flk_%s.json'%codes.split(',')[0],'w'),ensure_ascii=False)
print(len(recs), len(set(x['bbbs'] for x in recs)))
import collections
print(collections.Counter(x['flxz'] for x in recs).most_common(12))
print(sum(1 for x in recs if not x.get('gbrq')), 'without gbrq')
