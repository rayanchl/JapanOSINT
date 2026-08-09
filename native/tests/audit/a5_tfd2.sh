#!/usr/bin/env bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/126.0 Safari/537.36'
echo "=== homepage links containing saigai/disaster"
curl -sSL -m 25 -A "$UA" https://www.tfd.metro.tokyo.lg.jp/ -o /tmp/tfd_home.html -w '[%{http_code} %{size_download}]\n'
grep -oiE 'href="[^"]{0,120}"' /tmp/tfd_home.html | grep -iE 'saigai|disaster|kasai|shutsudo' | sort -u | head -20
echo "=== candidate 1"
curl -sSL -m 25 -A "$UA" -o /tmp/t1.html -w '[%{http_code}] trs=' https://www.tfd.metro.tokyo.lg.jp/lfe/saigai/saigai.html
grep -c '<tr' /tmp/t1.html
echo "=== sitemap"
curl -sSL -m 25 -A "$UA" https://www.tfd.metro.tokyo.lg.jp/sitemap.html 2>/dev/null | grep -oiE 'href="[^"]{0,120}"' | grep -i saigai | sort -u | head
