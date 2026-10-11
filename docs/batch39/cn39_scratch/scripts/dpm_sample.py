import json,time,urllib.request
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
def get(p):
    req=urllib.request.Request("https://zm-digicol.dpm.org.cn/cultural/queryList?page=%d&pageSize=200"%p,data=b'{}',headers={"User-Agent":UA,"Content-Type":"application/json"},method="POST")
    return [x['uuid'] for x in json.load(urllib.request.urlopen(req,timeout=60))['rows']]
pages=[1,2,3,500,501,4000,4001,9300]
a={p:get(p) for p in pages}; time.sleep(3)
b={p:get(p) for p in pages}
for p in pages: print(p, a[p]==b[p], len(set(a[p])))
allids=[u for p in pages for u in a[p]]
print('distinct over sample', len(allids), len(set(allids)))
