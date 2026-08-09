#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'
for u in 'https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20234&to=20234&area=13' \
         'https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20241&to=20244&area=13' \
         'https://www.japan-reit.com/meigara/' ; do
  echo "-- $u"
  curl -sL --max-time 30 -A "$UA" "$u" -o /tmp/p.txt -w '   %{http_code} %{size_download} ct=%{content_type}\n'
  head -c 400 /tmp/p.txt | tr '\n' ' '; echo
done
echo "-- japan-reit default UA"
curl -sL --max-time 30 'https://www.japan-reit.com/meigara/' -o /tmp/jr.txt -w '   %{http_code} %{size_download}\n'
head -c 300 /tmp/jr.txt | tr '\n' ' '; echo
