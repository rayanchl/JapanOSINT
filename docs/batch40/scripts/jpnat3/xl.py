import sys,re,csv,io,zipfile
sys.path.insert(0,'/Users/rayan/OSINTsaas/native/tools')
from probe_hp_batch import _xlsx_rows
fn=sys.argv[1]; sidx=int(sys.argv[2]) if len(sys.argv)>2 else 0; skip=int(sys.argv[3]) if len(sys.argv)>3 else 0
n=int(sys.argv[4]) if len(sys.argv)>4 else 6
raw=open(fn,'rb').read()
z=zipfile.ZipFile(io.BytesIO(raw))
wb=z.read('xl/workbook.xml').decode()
print('SHEETS',re.findall(r'<(?:\w+:)?sheet\b[^>]*?\bname="([^"]*)"',wb))
tab,rows=_xlsx_rows(raw,'',sidx)
out=[]
for cells in rows:
    line=[]
    for c in cells:
        if any(ch in c for ch in ',"\r\n'): c='"'+c.replace('"','""')+'"'
        line.append(c)
    out.append(",".join(line))
text="\n".join(out)+"\n"
for _ in range(skip):
    nl=text.find("\n"); text=text[nl+1:]
rd=list(csv.reader(io.StringIO(text)))
hdr=[h.strip() for h in rd[0]]
data=[r for r in rd[1:] if any(c.strip() for c in r)]
print('TAB',tab,'HEADER',hdr)
print('DATA',len(data))
for r in data[:n]: print(r)
print('LAST',data[-2:] if data else None)
if len(sys.argv)>5:
    keys=sys.argv[5].split('+')
    idx=[hdr.index(k) for k in keys]
    vals=[tuple(r[i] if i<len(r) else '' for i in idx) for r in data]
    print('IDKEYS',keys,'distinct',len(set(vals)),'of',len(vals), 'empty', sum(1 for v in vals if not any(v)))
