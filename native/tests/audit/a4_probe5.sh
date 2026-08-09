#!/bin/bash
echo "=== BOM nsw warnings feed"
curl -sL --max-time 25 http://www.bom.gov.au/fwo/IDZ00054.warnings_nsw.xml -o /tmp/b.xml -w '%{http_code} %{size_download}\n'
head -c 1400 /tmp/b.xml; echo
echo "=== vedur more"
for u in 'https://hraun.vedur.is/ja/skjalftalisti/skjalftar' 'https://api.vedur.is/skjalftalisti/v1/skjalftar?count=20' 'https://api.vedur.is/earthquake/v1/quake/' 'https://api.vedur.is/skjalftalisti/v1/' 'https://en.vedur.is/earthquakes-and-volcanism/earthquakes/#view=table'; do
  echo "-- $u"
  curl -skL --max-time 20 "$u" -o /tmp/v.txt -w '   %{http_code} %{size_download}\n'
  head -c 200 /tmp/v.txt | tr '\n' ' '; echo
done
