#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'
curl -sL --max-time 30 -A "$UA" 'https://www.japan-reit.com/' -o /tmp/jr.html
echo "== hrefs containing meigara/ichiran/list"
grep -oE 'href="[^"]+"' /tmp/jr.html | grep -iE 'meigara|ichiran|list|brand' | sort -u | head -30
echo "== all top-level dirs"
grep -oE 'href="/[a-z_-]+/' /tmp/jr.html | sort -u | head -40
echo "== mlit dns"
getent hosts www.land.mlit.go.jp || echo "no dns"
curl -sv --max-time 20 'https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20234&to=20234&area=13' -o /dev/null 2>&1 | tail -15
