import sys,collections
sys.path.insert(0,'/Users/rayan/OSINTsaas/native/tools')
from probe_hp_batch import _xlsx_rows
raw=open(sys.argv[1],'rb').read()
tab,rows=_xlsx_rows(raw,"",int(sys.argv[2]) if len(sys.argv)>2 else 0)
print("tab",tab,"rows",len(rows))
for r in rows[:4]: print(r)
print("last",rows[-1])
if len(sys.argv)>3:
    hdr=rows[0]; keys=sys.argv[3].split("+"); idx=[hdr.index(k) for k in keys]
    ks=[tuple(r[i] if i<len(r) else "" for i in idx) for r in rows[1:]]
    print("records",len(ks),"distinct",len(set(ks)), collections.Counter(ks).most_common(2))
