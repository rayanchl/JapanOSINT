#!/bin/bash
echo "=== NASA CMR AMSR2"
curl -sL --max-time 30 'https://cmr.earthdata.nasa.gov/search/granules.json?keyword=AMSR2&bounding_box=122.0,24.0,146.0,46.0&sort_key=-start_date&page_size=50' -o /tmp/cmr.json -w '%{http_code} %{size_download}\n'
head -c 500 /tmp/cmr.json; echo
echo "=== NASA CMR AMSR2 no bbox"
curl -sL --max-time 30 'https://cmr.earthdata.nasa.gov/search/granules.json?keyword=AMSR2&sort_key=-start_date&page_size=5' -o /tmp/cmr2.json -w '%{http_code} %{size_download}\n'
head -c 500 /tmp/cmr2.json; echo
echo "=== bird.makeup outbox"
curl -sL --max-time 25 -H 'accept: application/activity+json' -A 'JapanOSINT/1.0' 'https://bird.makeup/users/UN_NERV/outbox?page=true' -o /tmp/bm.json -w '%{http_code} %{size_download}\n'
head -c 400 /tmp/bm.json; echo
echo "=== bird.makeup root"
curl -sL --max-time 25 'https://bird.makeup/' -o /tmp/bm2 -w '%{http_code} %{size_download}\n'
head -c 300 /tmp/bm2 | tr '\n' ' '; echo
