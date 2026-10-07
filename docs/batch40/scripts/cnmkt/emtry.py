import json,sys
from em import em
for rep in sys.argv[1:]:
    try:
        url,d=em(rep,ps=1)
        r=d.get("result") or {}
        data=r.get("data") or []
        print("%-36s %s %s count=%s keys=%s"%(rep,d.get("success"),d.get("message"),r.get("count"),",".join(list(data[0].keys())[:14]) if data else ""))
    except Exception as e:
        print(rep,"ERR",e)
