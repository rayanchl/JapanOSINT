#!/usr/bin/env python3
TIMEOUTS = {"cell-towers","city-halls","crtsh-historical","ferry-routes",
            "overpass-subway-tracks","rice-paddies","sento-public-baths",
            "shrine-temple","submarine-cables","unified-trains"}
base = '/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/'
a, b = [], []
done = {"bom-au-warnings","copernicus-ems","emsc-quakes","iceland-quakes",
        "metoffice-warnings","nhc-cpac","pdc-disasters","volcanodiscovery",
        "bird-makeup-jp","kitco-news","trading-econ","gcom-w","japan-reit",
        "mlit-transaction","unesco-heritage"}
for line in open(base + 'a4_all.txt'):
    line = line.rstrip('\n')
    if not line.strip():
        continue
    sid = line.split(' ')[0]
    if sid in TIMEOUTS:
        b.append(line)
    elif sid not in done:
        a.append(line)
n = 4
chunks = [a[i::n] for i in range(n)]
for i, ch in enumerate(chunks):
    open(base + 'a4_A%d.txt' % i, 'w').write("\n".join(ch) + "\n")
    print('A%d' % i, len(ch))
open(base + 'a4_B.txt', 'w').write("\n".join(b) + "\n")
print('B', len(b))
