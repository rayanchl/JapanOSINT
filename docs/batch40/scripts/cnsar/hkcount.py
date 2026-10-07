import json,sys,urllib.request
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
def get(u):
    r=urllib.request.Request(u,headers={'User-Agent':UA})
    return json.load(urllib.request.urlopen(r,timeout=90))
base=sys.argv[1]; ps=int(sys.argv[2]) if len(sys.argv)>2 else 1000
allr=[];off=0
while True:
    sep='&' if '?' in base else '?'
    d=get(f"{base}{sep}pagesize={ps}&offset={off}")
    if not d['header']['success']: print('ERR',d['header']);break
    r=d['result']['records']; allr+=r
    print(' off',off,'got',len(r),file=sys.stderr)
    if len(r)<ps: break
    off+=ps; import time; time.sleep(1.5)
    if off>200000: break
json.dump(allr,open(sys.argv[3] if len(sys.argv)>3 else '/dev/null','w'),ensure_ascii=False)
print('TOTAL',len(allr))
if allr: print('keys',list(allr[0].keys()))
