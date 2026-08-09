#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36'
echo "=== BOM rss page raw sample"
curl -sL --max-time 25 -A "$UA" http://www.bom.gov.au/rss/ -o /tmp/bom.html -w '%{http_code} %{size_download}\n'
grep -oiE '(href|HREF)=[^ >]+' /tmp/bom.html | head -60
echo "=== pdc with browser UA"
curl -sL --max-time 25 -A "$UA" https://www.pdc.org/feed/ -o /tmp/pdc.xml -w '%{http_code} %{size_download}\n'
head -c 300 /tmp/pdc.xml | tr '\n' ' '; echo
echo "=== vedur candidates 2"
for u in 'https://api.vedur.is/skjalftalisti/v1/island' 'https://api.vedur.is/skjalftalisti/v1/skjalftar/1' 'https://en.vedur.is/earthquakes-and-volcanism/earthquakes/reykjanespeninsula/' 'https://apis.is/earthquake/is'; do
  echo "-- $u"
  curl -skL --max-time 20 "$u" -o /tmp/v.txt -w '   %{http_code} %{size_download}\n'
  head -c 250 /tmp/v.txt | tr '\n' ' '; echo
done
