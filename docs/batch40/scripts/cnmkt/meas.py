import json,sys,re,collections,argparse
from em import get
ap=argparse.ArgumentParser()
ap.add_argument("url"); ap.add_argument("--path",default=""); ap.add_argument("--ids",default=""); ap.add_argument("--title",default="")
ap.add_argument("--pp",default=""); ap.add_argument("--p2",default=""); ap.add_argument("--hdr",action="append",default=[]); ap.add_argument("--pages",type=int,default=2)
ap.add_argument("--show",type=int,default=1)
a=ap.parse_args()
H={}
for h in a.hdr:
    k,_,v=h.partition(":"); H[k]=v.strip()
def node(d,path):
    for seg in [s for s in path.split(".") if s]:
        if isinstance(d,list) and seg.isdigit(): d=d[int(seg)]
        elif isinstance(d,dict): d=d.get(seg)
        else: return None
    return d
def flat(x,pre="",out=None):
    if out is None: out={}
    if isinstance(x,dict):
        for k,v in x.items(): flat(v,pre+"."+k if pre else k,out)
    elif isinstance(x,list):
        for i,v in enumerate(x): flat(v,pre+"."+str(i) if pre else str(i),out)
    else: out[pre]=x
    return out
allr=[];pages=[]
for i in range(a.pages):
    u=a.url
    if i>0:
        if not a.pp: break
        v=a.p2.split(",")[i-1] if a.p2 else None
        u=re.sub(r"([?&]%s=)[^&]*"%re.escape(a.pp),lambda m:m.group(1)+v,a.url)
    st,b=get(u,H,timeout=90)
    d=json.loads(b)
    arr=node(d,a.path) if a.path else d
    n=len(arr) if isinstance(arr,list) else None
    print("page",i+1,"status",st,"bytes",len(b),"n",n)
    if not isinstance(arr,list): print(str(d)[:300]); break
    recs=[flat(r) for r in arr]; pages.append(recs); allr+=recs
    if i==0 and isinstance(d,dict):
        top={k:v for k,v in d.items() if not isinstance(v,(list,dict))}; print("top",str(top)[:300])
        if a.path and "." in a.path:
            par=node(d,".".join(a.path.split(".")[:-1]))
            if isinstance(par,dict): print("parent",str({k:v for k,v in par.items() if not isinstance(v,(list,dict))})[:300])
if allr and a.show: print(json.dumps(allr[0],ensure_ascii=False)[:1200])
if a.ids and allr:
    for spec in a.ids.split(";"):
        ks=spec.split("+")
        keys=[tuple(str(r.get(k)) for k in ks) for r in allr]
        miss=sum(1 for r in allr if all(r.get(k) in (None,"") for k in ks))
        print("ids",spec,"records",len(keys),"distinct",len(set(keys)),"allmissing",miss, collections.Counter(keys).most_common(2) if len(set(keys))<len(keys) else "")
    if len(pages)>1:
        ks=a.ids.split(";")[0].split("+")
        s=[set(tuple(str(r.get(k)) for k in ks) for r in p) for p in pages]
        print("overlap p1/p2",len(s[0]&s[1]))
if a.title and allr:
    for t in a.title.split(","):
        print("title",t,"present",sum(1 for r in allr if r.get(t) not in (None,"")),"/",len(allr), "e.g.",[allr[0].get(t)])
