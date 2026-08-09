#!/usr/bin/env bash
# Does the JR West Next.js page ship its data server-side, and does tenki.jp /
# JMA volcano still expose the markers the collectors look for?
set -u
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
g() { curl -s -A "$UA" -L --max-time 25 "$1"; }

echo "=== JR West: embedded payload? ==="
g 'https://trafficinfo.westjr.co.jp/' > /tmp/jrw.html
for k in __NEXT_DATA__ self.__next_f "運転" "遅延" "平常"; do
  printf '  %-16s %s\n' "$k" "$(grep -c -- "$k" /tmp/jrw.html)"
done
echo "  --- sample around 平常/遅延 ---"
grep -oE '.{0,80}(平常運転|遅延|運転見合わせ).{0,120}' /tmp/jrw.html | head -3

echo
echo "=== tenki.jp: forecast markers ==="
g 'https://tenki.jp/' > /tmp/tenki.html
for k in forecast-days today-weather weather-telop high-temp low-temp; do
  printf '  %-18s %s\n' "$k" "$(grep -c -- "$k" /tmp/tenki.html)"
done
grep -oE '<p class="weather-telop">[^<]*' /tmp/tenki.html | head -3
grep -oE 'class="high-temp[^"]*"[^>]*>.{0,60}' /tmp/tenki.html | head -3

echo
echo "=== JMA volcano page: table markers ==="
g 'https://www.data.jma.go.jp/vois/data/tokyo/volcano.html' > /tmp/vol.html
for k in "噴火警報" "警戒レベル" "<table" "volcano"; do
  printf '  %-14s %s\n' "$k" "$(grep -c -- "$k" /tmp/vol.html)"
done
grep -oE '<a href="[^"]*"[^>]*>[^<]{2,20}(山|岳|島)</a>' /tmp/vol.html | head -5
