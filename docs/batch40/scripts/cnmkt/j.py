import json,sys
from em import get
def show(url,hdr=None,n=600):
    try:
        st,b=get(url,hdr)
    except Exception as e:
        print("ERR",url,str(e)[:120]); return None
    print(st,len(b),url)
    try:
        d=json.loads(b)
    except Exception:
        print("  NOTJSON",b[:n]); return None
    print("  ",json.dumps(d,ensure_ascii=False)[:n]); return d
if __name__=="__main__":
    h={}
    for a in sys.argv[2:]:
        k,_,v=a.partition(":"); h[k]=v.strip()
    show(sys.argv[1],h,int(h.pop("N",600)) if "N" in h else 600)
