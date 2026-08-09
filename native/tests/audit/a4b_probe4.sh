#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
p() {
  lbl="$1"; url="$2"; shift 2
  code=$(curl -s -o /tmp/a4c.$$ -w '%{http_code} %{size_download} %{content_type}' \
         --max-time 25 -L "$@" "$url" 2>/dev/null)
  echo "--- $lbl  [$code]"
  [ -f /tmp/a4c.$$ ] && head -c 300 /tmp/a4c.$$ | tr '\n' ' '; echo
  rm -f /tmp/a4c.$$
}
echo "########## BoM"
p bom-1xml   "http://www.bom.gov.au/rss/1.xml" -A "$UA"
p bom-https  "https://www.bom.gov.au/rss/1.xml" -A "$UA"
p bom-warn   "http://www.bom.gov.au/australia/warnings/" -A "$UA"
p bom-cap    "https://api.weather.bom.gov.au/v1/warnings" -A "$UA"
echo "########## PDC"
p pdc-feed   "https://www.pdc.org/feed/" -A "$UA"
p pdc-news   "https://www.pdc.org/news/feed/" -A "$UA"
p pdc-root   "https://www.pdc.org/" -A "$UA"
echo "########## UNESCO variants"
p whs-xml-noslash "https://whc.unesco.org/en/list/xml" -A "$UA"
p whs-syndication "https://whc.unesco.org/en/syndication/" -A "$UA"
p whs-search-xml  "https://whc.unesco.org/en/list/xml/?&order=country" -A "$UA"
echo "########## MLIT"
p mlit-tx "https://www.land.mlit.go.jp/webland/api/TradeListSearch?from=20241&to=20244&area=13" -A "$UA"
p mlit-tx-new "https://www.reinfolib.mlit.go.jp/ex-api/external/XIT001?year=2024&area=13" -A "$UA"
p mlit-lp "https://www.land.mlit.go.jp/webland/api/CitySearch?area=13" -A "$UA"
