#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'
p() { echo "-- $1"; rm -f /tmp/p.txt; curl -sL --max-time 30 -A "$UA" "$1" -o /tmp/p.txt -w '   %{http_code} %{size_download} ct=%{content_type}\n'; head -c 300 /tmp/p.txt | tr '\n' ' '; echo; }
p 'https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20234&to=20234&area=13'
p 'https://www.land.mlit.go.jp/webland/'
p 'https://www.japan-reit.com/'
p 'https://www.japan-reit.com/meigara'
p 'https://www.japan-reit.com/list/meigara/'
p 'https://www.japan-reit.com/ichiran/'
