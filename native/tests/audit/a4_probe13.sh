#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'
curl -sL --max-time 30 -A "$UA" 'https://www.japan-reit.com/list/rimawari/' -o /tmp/rm.html -w '%{http_code} %{size_download}\n'
python3 - <<'EOF'
import re
d=open('/tmp/rm.html',encoding='utf-8',errors='replace').read()
links=re.findall(r'<a[^>]+href="/meigara/([0-9A-Za-z]{4})/?"[^>]*>([^<]{2,})</a>',d)
print("meigara anchors:",len(links))
for a in links[:8]: print("  ",a)
i=d.find('<table')
print(d[i:i+1200])
EOF
