#!/bin/bash
echo "=== BOM rss page hrefs"
curl -sL --max-time 25 http://www.bom.gov.au/rss/ | grep -oiE 'href="[^"]+"' | grep -iE 'xml|rss|feed' | sort -u | head -40
echo "=== copernicus /latest/feed/"
curl -sL --max-time 25 'https://mapping.emergency.copernicus.eu/latest/feed/' -o /tmp/cop.xml -w '%{http_code} %{size_download}\n'
head -c 700 /tmp/cop.xml; echo
echo "=== emsc rss candidates"
for u in 'https://www.emsc-csem.org/service/rss/rss.php?typ=emsc&min_mag=4' 'https://www.emsc.eu/Earthquake/?view=rss' 'https://www.emsc-csem.org/index.php?view=rss' 'https://www.seismicportal.eu/fdsnws/event/1/query?limit=20&format=json&orderby=time'; do
  echo "-- $u"
  curl -sL --max-time 20 "$u" -o /tmp/e.xml -w '   %{http_code} %{size_download}\n'
  head -c 200 /tmp/e.xml | tr '\n' ' '; echo
done
echo "=== vedur candidates"
for u in 'https://en.vedur.is/earthquakes-and-volcanism/earthquakes/rss' 'https://www.vedur.is/skjalftar-og-eldgos/jardskjalftar/rss/' 'https://hraun.vedur.is/ja/viku/vikuskjalftar.xml' 'https://api.vedur.is/skjalftalisti/v1/skjalftar'; do
  echo "-- $u"
  curl -sL --max-time 20 "$u" -o /tmp/v.xml -w '   %{http_code} %{size_download}\n'
  head -c 200 /tmp/v.xml | tr '\n' ' '; echo
done
