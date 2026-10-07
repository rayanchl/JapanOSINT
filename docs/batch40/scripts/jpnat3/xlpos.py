# position-aware xlsx reader (like lib/xlsx.c): prints header (row index given) and first rows, key uniqueness
import sys,re,zipfile,io
def col(ref):
    c=0
    for ch in re.match(r'[A-Z]+',ref).group(0): c=c*26+ord(ch)-64
    return c-1
def unesc(s): return s.replace('&lt;','<').replace('&gt;','>').replace('&quot;','"').replace('&apos;',"'").replace('&amp;','&')
def rows(fn,sidx=0):
    z=zipfile.ZipFile(fn); names=z.namelist()
    wb=z.read('xl/workbook.xml').decode()
    sheets=re.findall(r'<(?:\w+:)?sheet\b[^>]*?\bname="([^"]*)"[^>]*?\br:id="([^"]*)"',wb)
    rx=z.read('xl/_rels/workbook.xml.rels').decode()
    rels={}
    for m in re.finditer(r'<Relationship\b([^>]*)/?>',rx):
        a=m.group(1); i=re.search(r'Id="([^"]*)"',a).group(1); t=re.search(r'Target="([^"]*)"',a).group(1); rels[i]=t
    tab,rid=sheets[sidx]; part=rels[rid].lstrip('/'); part=part if part.startswith('xl/') else 'xl/'+part
    ss=[]
    if 'xl/sharedStrings.xml' in names:
        sx=z.read('xl/sharedStrings.xml').decode()
        for si in re.findall(r'<si>(.*?)</si>',sx,re.S):
            si=re.sub(r'<rPh\b.*?</rPh>','',si,flags=re.S); ss.append(unesc(''.join(re.findall(r'<t\b[^>]*>(.*?)</t>',si,re.S))))
    sx=z.read(part).decode()
    out=[]
    for r in re.findall(r'<row\b[^>]*>(.*?)</row>|<row\b[^>]*/>',sx,re.S):
        cells={}
        for m in re.finditer(r'<c\b([^>]*?)(?:/>|>(.*?)</c>)',r,re.S):
            attrs,inner=m.group(1),m.group(2) or ''
            ref=re.search(r'\br="([A-Z]+)\d+"',attrs); 
            if not ref: continue
            ci=col(ref.group(1)); t=re.search(r'\bt="(\w+)"',attrs); v=re.search(r'<v>(.*?)</v>',inner,re.S)
            if t and t.group(1)=='s' and v: val=ss[int(v.group(1))]
            elif v: val=unesc(v.group(1))
            else: val=unesc(''.join(re.findall(r'<t\b[^>]*>(.*?)</t>',re.sub(r'<rPh\b.*?</rPh>','',inner,flags=re.S),re.S)))
            cells[ci]=val
        n=max(cells)+1 if cells else 0
        out.append([cells.get(i,'') for i in range(n)])
    return tab,out
if __name__=='__main__':
    fn=sys.argv[1]; sidx=int(sys.argv[2]); hrow=int(sys.argv[3]); keys=sys.argv[4].split('+') if len(sys.argv)>4 else None
    tab,rs=rows(fn,sidx)
    h=rs[hrow]; d=[r for r in rs[hrow+1:] if any(c.strip() for c in r)]
    print('TAB',tab,'HEADER',h); print('DATA',len(d))
    for r in d[:3]: print(r)
    print('LAST',d[-1])
    if keys:
        idx=[h.index(k) for k in keys]
        vals=[tuple(r[i] if i<len(r) else '' for i in idx) for r in d]
        print('KEYS',keys,'distinct',len(set(vals)),'of',len(vals))
