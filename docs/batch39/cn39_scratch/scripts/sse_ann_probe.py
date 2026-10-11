import json, time, urllib.request
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
B="https://query.sse.com.cn/security/stock/queryCompanyBulletinNew.do?isPagination=true&pageHelp.pageSize=25&pageHelp.pageNo=%d&pageHelp.beginPage=%d&pageHelp.cacheSize=1&pageHelp.endPage=%d&START_DATE=%s&END_DATE=%s&SECURITY_CODE=&TITLE=&BULLETIN_TYPE=&stockType="
def get(p,s,e):
    req=urllib.request.Request(B%(p,p,p,s,e),headers={"User-Agent":UA,"Referer":"https://www.sse.com.cn/"})
    return json.load(urllib.request.urlopen(req,timeout=60))
for day in ["2026-09-28","2026-09-29","2026-09-30"]:
    d=get(1,day,day); print(day,'total',d['pageHelp']['total'],'pages',d['pageHelp']['pageCount']); time.sleep(2)
groups=[]
for p in range(1,6):
    d=get(p,"2026-09-28","2026-10-06"); g=d['pageHelp']['data']; groups+=g
    print('page',p,len(g),g[0][0]['ORG_BULLETIN_ID'],g[0][0]['SSEDATE'],g[-1][0]['SSEDATE']); time.sleep(2)
ids=[g[0]['ORG_BULLETIN_ID'] for g in groups]
print(len(ids),len(set(ids)))
urls=[g[0]['URL'] for g in groups]; print('url distinct',len(set(urls)))
print(sum(1 for g in groups if len(set(x['ORG_BULLETIN_ID'] for x in g))>1),'groups with mixed ids')
