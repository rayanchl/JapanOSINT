import sys,re,xml.etree.ElementTree as ET,email.utils,datetime,collections
f=sys.argv[1]
raw=open(f,'rb').read()
try:
    root=ET.fromstring(raw)
except Exception as e:
    print('PARSE ERR',e, raw[:200]); sys.exit()
items=root.findall('.//item') or root.findall('.//{http://www.w3.org/2005/Atom}entry')
ds=[]
for it in items:
    for tag in ('pubDate','{http://purl.org/dc/elements/1.1/}date','{http://www.w3.org/2005/Atom}updated'):
        e=it.find(tag)
        if e is not None and e.text:
            t=e.text.strip()
            try: d=email.utils.parsedate_to_datetime(t)
            except Exception:
                try: d=datetime.datetime.fromisoformat(t.replace('Z','+00:00'))
                except Exception:
                    try: d=datetime.datetime.strptime(t[:19],'%Y-%m-%d %H:%M:%S')
                    except Exception: d=None
            if d: ds.append(d.replace(tzinfo=None) if d.tzinfo is None else d.astimezone(datetime.timezone.utc).replace(tzinfo=None))
            break
links=[ (it.findtext('guid') or it.findtext('link') or '') for it in items]
print('items',len(items),'distinct guid/link',len(set(links)),end=' ')
if ds:
    ds.sort(); span=(ds[-1]-ds[0]).total_seconds()/86400
    last24=sum(1 for d in ds if (ds[-1]-d).total_seconds()<86400)
    print('newest',ds[-1],'oldest',ds[0],'span_days %.2f'%span,'in_last24h_of_newest',last24)
else: print('no dates')
