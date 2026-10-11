import json,time,urllib.request,sys
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
plate=sys.argv[1]; win=sys.argv[2]
recs=[]; total=None
for p in range(1,101):
    body=("pageNum=%d&pageSize=30&column=szse&tabName=fulltext&stock=&searchkey=&secid=&category=&trade=&seDate=%s&sortName=&sortType=&isHLtitle=true"%(p,win)).encode()
    req=urllib.request.Request("https://www.cninfo.com.cn/new/hisAnnouncement/query?plate="+plate,data=body,headers={"User-Agent":UA,"Content-Type":"application/x-www-form-urlencoded"},method="POST")
    d=json.load(urllib.request.urlopen(req,timeout=60)); a=d.get('announcements')
    total=total or d.get('totalAnnouncement')
    if not a: print('end at page',p,file=sys.stderr); break
    recs+=a; time.sleep(1)
ids=[x['announcementId'] for x in recs]
print(plate,win,'total',total,'got',len(recs),'distinct',len(set(ids)))
