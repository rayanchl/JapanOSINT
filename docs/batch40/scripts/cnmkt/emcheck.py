import json,sys,collections
from em import em
rep=sys.argv[1]; sort=sys.argv[2]; keys=sys.argv[3].split("+"); extra=sys.argv[4] if len(sys.argv)>4 else ""
pages=int(sys.argv[5]) if len(sys.argv)>5 else 2
allrecs=[]; per=[]
for pn in range(1,pages+1):
    url,d=em(rep,pn=pn,sort=sort,extra=extra)
    r=d.get("result") or {}
    data=r.get("data") or []
    per.append(data); allrecs+=data
    print("page",pn,"n",len(data),"pages",r.get("pages"),"count",r.get("count"))
def k(x): return tuple(str(x.get(f)) for f in keys)
def kf(x): return tuple(str(float(x.get(f))) if isinstance(x.get(f),int) and abs(x.get(f))>2**53 else str(x.get(f)) for f in keys)
ks=[k(x) for x in allrecs]
print("records",len(ks),"distinct",len(set(ks)),"distinct-as-double",len(set(kf(x) for x in allrecs)))
if pages>=2:
    s1=set(k(x) for x in per[0]); s2=set(k(x) for x in per[1]); print("overlap p1/p2",len(s1&s2))
c=collections.Counter(ks); print("top dups",c.most_common(3))
print("dates", allrecs[0].get(sort.split(",")[0]) if sort else "", allrecs[-1].get(sort.split(",")[0]) if sort else "")
