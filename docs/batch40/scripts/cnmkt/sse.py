import json,sys,collections
from em import get
H={"Referer":"https://www.sse.com.cn/"}
def q(url):
    st,b=get(url,H,timeout=60); return json.loads(b)
if __name__=="__main__":
    base=sys.argv[1]; keys=sys.argv[2].split("+") if len(sys.argv)>2 else ["docId"]
    pn=sys.argv[3] if len(sys.argv)>3 else "pageHelp.pageNo"
    import re
    recs=[];firsts=[]
    for p in (1,2):
        u=re.sub(r"(%s=)\d+"%re.escape(pn),r"\g<1>%d"%p,base)
        d=q(u); ph=d.get("pageHelp") or {}
        data=ph.get("data") or d.get("result") or []
        print("page",p,"total",ph.get("total"),"pageCount",ph.get("pageCount"),"pageSize",ph.get("pageSize"),"n",len(data))
        firsts.append(json.dumps(data[0],ensure_ascii=False)[:300] if data else None)
        recs.append(data)
    print(firsts[0]); print(firsts[1])
    k=lambda x: tuple(str(x.get(f)) for f in keys)
    a=[k(x) for x in recs[0]]; b=[k(x) for x in recs[1]]
    print("p1 distinct",len(set(a)),"/",len(a),"overlap",len(set(a)&set(b)))
