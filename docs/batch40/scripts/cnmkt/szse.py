import json,sys
from em import get
for c in sys.argv[1:]:
    url="https://www.szse.cn/api/report/ShowReport/data?SHOWTYPE=JSON&CATALOGID=%s&PAGENO=1"%c
    try:
        st,b=get(url)
        d=json.loads(b)
    except Exception as e:
        print(c,"ERR",str(e)[:80]); continue
    if isinstance(d,dict): print(c,"dict",str(d)[:150]); continue
    for t in d:
        if not isinstance(t,dict): print(c,"nonobj",str(t)[:100]); continue
        md=t.get('metadata',{})
        print(c,md.get('tabkey'),md.get('name'),md.get('subname'),"pages",md.get('pagecount'),"recs",md.get('recordcount'),len(t.get('data') or []), str(md.get("cols"))[:200])
