#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
p() { # p <label> <url> [extra curl args...]
  lbl="$1"; url="$2"; shift 2
  code=$(curl -s -o /tmp/a4b.$$ -w '%{http_code} %{size_download} %{content_type}' \
         --max-time 25 -L "$@" "$url")
  echo "--- $lbl  [$code]"
  head -c 260 /tmp/a4b.$$ | tr '\n' ' '; echo
  rm -f /tmp/a4b.$$
}
echo "########## kitco"
p kitco-rss "https://www.kitco.com/rss/KitcoNews.xml"
p kitco-news-feed "https://www.kitco.com/news/feed" -A "$UA"
p kitco-rss2 "https://www.kitco.com/rss/" -A "$UA"
echo "########## bom au"
p bom-plain "ftp://ftp.bom.gov.au/anon/gen/fwo/IDZ00054.warnings_land_nsw.xml"
p bom-rss "http://www.bom.gov.au/fwo/IDZ00054.warnings_land_nsw.xml" -A "$UA"
p bom-cap "http://www.bom.gov.au/fwo/IDZ00060.warnings_marine.xml" -A "$UA"
echo "########## pdc"
p pdc "https://sentry.pdc.org/hp_srv/services/hazards/t/json/get_active_hazards" -A "$UA"
p pdc2 "https://sentry.pdc.org/hp_srv/services/hazard/t/json/get_active_hazards" -A "$UA"
echo "########## unesco"
p unesco "https://whc.unesco.org/en/list/xml/" -A "$UA"
p unesco2 "https://whc.unesco.org/en/list/rss/" -A "$UA"
echo "########## metoffice"
p metoffice "https://www.metoffice.gov.uk/public/data/PWSCache/WarningsRSS/Region/UK" -A "$UA"
echo "########## nhc cpac"
p nhccp "https://www.nhc.noaa.gov/index-cp.xml" -A "$UA"
