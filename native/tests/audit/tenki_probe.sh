#!/usr/bin/env bash
# What does tenki.jp's forecast block look like today, and what does the
# collector currently look for?
set -u
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
curl -s -A "$UA" -L --max-time 25 'https://tenki.jp/forecast/3/16/' -o /tmp/tk.html
echo "size=$(wc -c </tmp/tk.html)"
echo "=== class histogram (forecast-ish) ==="
grep -oE 'class="[a-zA-Z0-9 _-]*"' /tmp/tk.html \
  | grep -iE 'weather|telop|temp|forecast|today|day' | sort | uniq -c | sort -rn | head -20
echo "=== first forecast block ==="
grep -oE '<section class="[^"]*today-weather[^"]*".{0,900}' /tmp/tk.html | head -1
echo "=== telop-ish text ==="
grep -oE '<p class="[^"]*weather-telop[^"]*">[^<]*' /tmp/tk.html | head -4
grep -oE 'alt="[^"]{2,12}"' /tmp/tk.html | sort -u | head -12
echo "=== temps ==="
grep -oE '<span class="[^"]*temp[^"]*">[^<]*' /tmp/tk.html | head -8
