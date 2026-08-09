#!/bin/bash
python3 - <<'EOF'
import re, urllib.request
for u in ('https://www.japan-reit.com/list/rimawari/', 'https://www.japan-reit.com/'):
    req = urllib.request.Request(u, headers={'User-Agent': 'Mozilla/5.0'})
    d = urllib.request.urlopen(req, timeout=30).read().decode('utf-8', 'replace')
    m = re.findall(r'<a[^>]+href="/meigara/[0-9A-Za-z]{4}/?"[^>]*>.{0,150}?</a>', d, re.S)
    print(u, 'anchors:', len(m))
    for x in m[:5]:
        print('  ', repr(x[:160]))
EOF
