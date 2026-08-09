#!/bin/bash
echo "=== CMR collections AMSR2"
curl -sL --max-time 30 'https://cmr.earthdata.nasa.gov/search/collections.json?keyword=AMSR2&page_size=20' -o /tmp/c.json -w '%{http_code} %{size_download}\n'
python3 -c "
import json;d=json.load(open('/tmp/c.json'))
for e in d['feed']['entry'][:20]: print(e.get('short_name'),'|',e.get('version_id'),'|',e.get('dataset_id','')[:70],'|',e.get('id'))
" 2>&1 | head -30
echo
echo "=== CMR granules with short_name"
for sn in AU_SI25 AU_SI12 A2_DySno_NRT AU_Land; do
  echo "-- $sn"
  curl -sL --max-time 30 "https://cmr.earthdata.nasa.gov/search/granules.json?short_name=$sn&sort_key=-start_date&page_size=3" -o /tmp/g.json -w '   %{http_code} %{size_download}\n'
  head -c 300 /tmp/g.json; echo
done
echo "=== bird.makeup handles"
for h in UN_NERV earthquakejapan japantimes NHK_PR; do
  echo "-- $h"
  curl -sL --max-time 25 -H 'accept: application/activity+json' -A 'JapanOSINT/1.0' "https://bird.makeup/users/$h/outbox?page=true" -o /tmp/b.json -w '   %{http_code} %{size_download}\n'
  head -c 300 /tmp/b.json; echo
  curl -sL --max-time 25 -H 'accept: application/activity+json' -A 'JapanOSINT/1.0' "https://bird.makeup/users/$h/outbox" -o /tmp/b2.json -w '   nopage %{http_code} %{size_download}\n'
  head -c 300 /tmp/b2.json; echo
done
