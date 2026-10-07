import re,sys,urllib.request,ssl
ctx=ssl.create_default_context(); ctx.check_hostname=False; ctx.verify_mode=ssl.CERT_NONE
for u in sys.argv[1:]:
    try:
        s=urllib.request.urlopen(urllib.request.Request(u,headers={'User-Agent':'JapanOSINT/1.0'}),timeout=40,context=ctx).read().decode('utf-8','replace')
    except Exception as e:
        print(u,'ERR',e); continue
    print('##',u,len(s))
    found=set()
    for m in re.finditer(r'url\s*:\s*([^,\n]{0,160})',s): found.add('url: '+m.group(1).strip())
    for m in re.finditer(r'["\']((?:https?:)?/[A-Za-z0-9_./!-]*(?:api|Api|API|ajax|Ajax|query|Query|list|List|get|Get|search|Search|\.do|\.action|\.ashx|\.json)[A-Za-z0-9_./!?=&-]*)["\']',s): found.add(m.group(1))
    for f in sorted(found)[:80]: print('  ',f)
