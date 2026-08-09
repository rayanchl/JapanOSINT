#!/bin/bash
curl -sL --max-time 40 -A 'Mozilla/5.0' https://whc.unesco.org/en/list/rss/ -o /tmp/u.xml
python3 - <<'EOF'
d = open('/tmp/u.xml', encoding='utf-8', errors='replace').read()
i = d.find('<item')
print(d[i:i+1600])
print("---- item count:", d.count('<item'))
print("---- has geo:", 'georss' in d.lower(), 'latitude' in d.lower(), 'lat>' in d.lower())
EOF
