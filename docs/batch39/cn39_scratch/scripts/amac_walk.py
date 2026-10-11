import json,time,urllib.request,urllib.error,sys
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
base=sys.argv[1]; size=int(sys.argv[2]); out=sys.argv[3]; pause=float(sys.argv[4])
recs=[]
for p in range(0,400):
    for attempt in range(3):
        try:
            req=urllib.request.Request(base%(p,size),data=b'{}',headers={"User-Agent":UA,"Content-Type":"application/json"},method="POST")
            d=json.load(urllib.request.urlopen(req,timeout=120)); break
        except urllib.error.HTTPError as e:
            print('page',p,'attempt',attempt,'HTTP',e.code,file=sys.stderr); time.sleep(5*(attempt+1)); d=None
    if d is None: print('gave up at',p,file=sys.stderr); break
    c=d.get('content') or []
    print(p,len(c),d.get('totalElements'),file=sys.stderr)
    if not c: break
    recs+=c; time.sleep(pause)
json.dump(recs,open(out,'w'),ensure_ascii=False)
print(out,len(recs),len(set(x['id'] for x in recs)))
