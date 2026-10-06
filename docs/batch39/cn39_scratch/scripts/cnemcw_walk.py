import json,time,urllib.request
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
rows=[]
for p in range(1,31):
    body=("AreaID=&RiverID=&MNName=&PageIndex=%d&PageSize=60&action=getRealDatas"%p).encode()
    req=urllib.request.Request("https://szzdjc.cnemc.cn:8070/GJZ/Ajax/Publish.ashx",data=body,headers={"User-Agent":UA,"Content-Type":"application/x-www-form-urlencoded"},method="POST")
    d=json.load(urllib.request.urlopen(req,timeout=90)); tb=d.get('tbody') or []
    print(p,len(tb),d.get('total'),d.get('records'))
    if not tb: break
    rows+=tb; time.sleep(2)
print('sum',len(rows), len(set((r[0],r[2],r[3]) for r in rows)))
