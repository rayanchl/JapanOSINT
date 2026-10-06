# replica of lib/xlsx.c rendering (blank lines for missing rows, padded columns) + csv_skip_lines + header
import sys,re,zipfile,io,csv
sys.path.insert(0,'/Users/rayan/OSINTsaas/native/tools')
from manifest import parse_opts
def col(ref):
    c=0
    for ch in re.match(r'[A-Z]+',ref).group(0): c=c*26+ord(ch)-64
    return c
def unesc(s): return s.replace('&lt;','<').replace('&gt;','>').replace('&quot;','"').replace('&apos;',"'").replace('&amp;','&')
def render(raw,sidx):
    z=zipfile.ZipFile(io.BytesIO(raw)); names=z.namelist()
    wb=z.read('xl/workbook.xml').decode()
    sheets=re.findall(r'<(?:\w+:)?sheet\b[^>]*?\bname="([^"]*)"[^>]*?\br:id="([^"]*)"',wb)
    rx=z.read('xl/_rels/workbook.xml.rels').decode(); rels={}
    for m in re.finditer(r'<Relationship\b([^>]*)/?>',rx):
        a=m.group(1); rels[re.search(r'Id="([^"]*)"',a).group(1)]=re.search(r'Target="([^"]*)"',a).group(1)
    part=rels[sheets[sidx][1]].lstrip('/'); part=part if part.startswith('xl/') else 'xl/'+part
    ss=[]
    if 'xl/sharedStrings.xml' in names:
        sx=z.read('xl/sharedStrings.xml').decode()
        for si in re.findall(r'<si>(.*?)</si>',sx,re.S):
            si=re.sub(r'<rPh\b.*?</rPh>','',si,flags=re.S); ss.append(unesc(''.join(re.findall(r'<t\b[^>]*>(.*?)</t>',si,re.S))))
    sx=z.read(part).decode()
    rows=[]; maxc=0
    for m in re.finditer(r'<row\b([^>]*?)(?:/>|>(.*?)</row>)',sx,re.S):
        rn=int(re.search(r'\br="(\d+)"',m.group(1)).group(1)); cells={}
        for c in re.finditer(r'<c\b([^>]*?)(?:/>|>(.*?)</c>)',m.group(2) or '',re.S):
            a,inner=c.group(1),c.group(2) or ''
            ci=col(re.search(r'\br="([A-Z]+)\d+"',a).group(1)); t=re.search(r'\bt="(\w+)"',a); v=re.search(r'<v>(.*?)</v>',inner,re.S)
            if t and t.group(1)=='inlineStr': val=unesc(''.join(re.findall(r'<t\b[^>]*>(.*?)</t>',inner,re.S)))
            elif v and t and t.group(1)=='s': val=ss[int(v.group(1))]
            elif v: val=unesc(v.group(1))
            else: val=''
            cells[ci]=val; maxc=max(maxc,ci)
        rows.append((rn,cells))
    out=[]; cur=0
    for rn,cells in rows:
        while cur+1<rn: out.append(''); cur+=1
        cur=rn
        if cells:
            line=[]
            for i in range(1,maxc+1):
                s=cells.get(i,'')
                if any(ch in s for ch in ',"\r\n'): s='"'+s.replace('"','""')+'"'
                line.append(s)
            out.append(','.join(line))
        else: out.append('')
    return '\n'.join(out)+'\n'
def check(fn, opts):
    o,_,_=parse_opts(opts)
    text=render(open(fn,'rb').read(), int(o.get('xlsx_sheet_index','0') or 0))
    for _ in range(int(o.get('csv_skip_lines','0') or 0)):
        nl=text.find('\n'); text=text[nl+1:]
    rd=list(csv.reader(io.StringIO(text)))
    h=[x.strip() for x in rd[0]]; d=rd[1:]
    keys=[k.strip() for kk in ('title_keys','id_keys') for part in (o.get(kk) or '').split(',') for k in part.split('+') if k.strip()]
    miss=[k for k in keys if k not in h]
    print(fn,'header',h[:6],'records',len(d),'missing',miss)
    if o.get('id_keys'):
        first=o['id_keys'].split(',')[0].split('+')
        idx=[h.index(k) for k in first if k in h]
        vals=[tuple(r[i] if i<len(r) else '' for i in idx) for r in d]
        print('   id distinct',len(set(vals)),'of',len(vals),'empty',sum(1 for v in vals if not any(v)))
    else:
        firsts=[next((c for c in r if c.strip()),None) for r in d]
        print('   first-scalar distinct',len(set(firsts)),'of',len(firsts))
if __name__=='__main__':
    check(sys.argv[1], sys.argv[2])
