import json,sys,urllib.request,urllib.parse,collections
UA="JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)"
def get(url,hdr=None,timeout=40):
    h={"User-Agent":UA,"Accept":"*/*"}
    if hdr: h.update(hdr)
    req=urllib.request.Request(url,headers=h)
    with urllib.request.urlopen(req,timeout=timeout) as r:
        return r.status,r.read()
def em(report,ps=500,pn=1,extra="",sort=""):
    url="https://datacenter-web.eastmoney.com/api/data/v1/get?reportName=%s&columns=ALL&pageSize=%d&pageNumber=%d%s%s"%(report,ps,pn,("&sortColumns="+sort) if sort else "",extra)
    st,b=get(url)
    d=json.loads(b)
    return url,d
if __name__=="__main__":
    rep=sys.argv[1]; sort=sys.argv[2] if len(sys.argv)>2 else ""; extra=sys.argv[3] if len(sys.argv)>3 else ""
    url,d=em(rep,sort=sort,extra=extra)
    r=d.get("result") or {}
    data=r.get("data") or []
    print(url); print("success",d.get("success"),d.get("message"),"pages",r.get("pages"),"count",r.get("count"),"n",len(data))
    if data:
        print(json.dumps(data[0],ensure_ascii=False)[:1500])
        json.dump(d,open("em_%s.json"%rep,"w"),ensure_ascii=False)
