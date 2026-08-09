#!/bin/bash
echo "=== reddit"
curl -s -o /dev/null -w 'reddit new.json: %{http_code}\n' --max-time 20 \
  "https://www.reddit.com/r/japan/new.json?limit=25"
echo "=== peeringdb fac/net status"
for seg in fac net; do
  curl -s -o /tmp/pd_$seg -w "$seg: %{http_code} %{size_download}\n" --max-time 40 \
    -H 'accept: application/json' \
    "https://www.peeringdb.com/api/$seg?country=JP&depth=0&limit=1000"
done
echo "=== npa wanted index"
curl -s -o /dev/null -w 'npa shimeitehai: %{http_code}\n' --max-time 20 \
  "https://www.npa.go.jp/sousa/shimeitehai/index.html"
echo "=== egov public comment / advisory councils / waste haulers"
curl -s -o /dev/null -w 'e-gov pubcom: %{http_code}\n' --max-time 20 \
  "https://public-comment.e-gov.go.jp/servlet/Public"
