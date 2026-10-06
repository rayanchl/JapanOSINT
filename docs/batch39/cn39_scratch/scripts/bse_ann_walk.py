import json,time,urllib.request,sys
sys.path.insert(0,'/Users/rayan/OSINTsaas/native/tools')
from probe_hp_batch import jsonp_inner
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
s,e=sys.argv[1],sys.argv[2]; recs=[]
for p in range(0,300):
    body=("disclosureType[]=5&page=%d&companyCd=&isNewThree=1&startTime=%s&endTime=%s&keyword=&xxfcbj[]=2&hyType=&needFields[]=companyCd&needFields[]=companyName&needFields[]=disclosureTitle&needFields[]=disclosurePostTitle&needFields[]=destFilePath&needFields[]=publishDate&needFields[]=xxfcbj&needFields[]=fileExt&needFields[]=xxzrlx"%(p,s,e)).encode()
    req=urllib.request.Request("https://www.bse.cn/disclosureInfoController/companyAnnouncement.do",data=body,headers={"User-Agent":UA,"Content-Type":"application/x-www-form-urlencoded"},method="POST")
    d=json.loads(jsonp_inner(urllib.request.urlopen(req,timeout=60).read().decode('utf-8')))
    li=d[0]['listInfo']; c=li['content']
    if p==0: print('total',li['totalElements'],li['totalPages'],file=sys.stderr)
    if not c: print('empty at',p,file=sys.stderr); break
    recs+=c; time.sleep(1.2)
print(s,e,len(recs),len(set(x['destFilePath'] for x in recs)), min(x['publishDate'] for x in recs), max(x['publishDate'] for x in recs))
