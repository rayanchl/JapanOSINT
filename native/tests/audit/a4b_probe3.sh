#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
curl -s --max-time 40 -A "$UA" "https://whc.unesco.org/en/list/rss/" -o /tmp/whs.xml
echo "bytes: $(wc -c < /tmp/whs.xml)"
echo "--- <row> count: $(grep -o '<row>' /tmp/whs.xml | wc -l)"
echo "--- iso_code jp count: $(grep -o '<iso_code>jp</iso_code>' /tmp/whs.xml | wc -l)"
echo "--- first row:"
python3 - <<'EOF'
import re
t = open('/tmp/whs.xml', encoding='utf-8', errors='replace').read()
rows = re.findall(r'<row>(.*?)</row>', t, re.S)
print('rows', len(rows))
if rows:
    print(rows[0][:1800])
jp = [r for r in rows if '<iso_code>jp</iso_code>' in r]
print('jp rows', len(jp))
if jp:
    print('--- first JP row:')
    print(jp[0][:1500])
EOF
