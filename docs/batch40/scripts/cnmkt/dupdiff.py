import json,sys,collections
from em import em
rep=sys.argv[1]; sort=sys.argv[2]; keys=sys.argv[3].split("+"); extra=sys.argv[4]; pages=int(sys.argv[5])
recs=[]
for pn in range(1,pages+1):
    url,d=em(rep,pn=pn,sort=sort,extra=extra); recs+=(d.get("result") or {}).get("data") or []
g=collections.defaultdict(list)
for x in recs: g[tuple(str(x.get(f)) for f in keys)].append(x)
n=0
for kk,v in g.items():
    if len(v)>1:
        diff={f:[y.get(f) for y in v] for f in v[0] if len(set(json.dumps(y.get(f),ensure_ascii=False) for y in v))>1}
        print(kk, json.dumps(diff,ensure_ascii=False)[:400]); n+=1
        if n>6: break
for f in recs[0]:
    print(f, len(set(json.dumps(x.get(f)) for x in recs)))
