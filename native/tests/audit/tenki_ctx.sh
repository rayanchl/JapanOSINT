#!/usr/bin/env bash
# Show the context around tenki.jp's REAL temperature values, so the collector
# pairs a place with a number that actually exists in the served HTML.
set -u
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
curl -s -A "$UA" -L --max-time 25 'https://tenki.jp/forecast/3/16/' -o /tmp/tk.html
echo "=== max-temp occurrences with value ==="
grep -oE 'class="max-temp">[^<]{1,8}' /tmp/tk.html | head -10
echo "=== 600 chars BEFORE the first numeric max-temp ==="
python3 - <<'PY'
import re
h = open('/tmp/tk.html', encoding='utf-8', errors='replace').read()
m = re.search(r'class="max-temp">\s*(\d+)', h)
if not m:
    print("no numeric max-temp found"); raise SystemExit
s = max(0, m.start() - 700)
print(re.sub(r'\s+', ' ', h[s:m.end() + 200]))
PY
