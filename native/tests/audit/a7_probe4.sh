#!/usr/bin/env bash
echo "=== note v2 hashtags/trending ==="
curl -sL -m 20 -A japanosint-collector -H 'accept: application/json' \
  https://note.com/api/v2/hashtags/trending | head -c 900; echo
echo; echo "=== nhkworld all.json ==="
curl -sL -m 20 -A japanosint-collector -H 'accept: application/json' \
  https://www3.nhk.or.jp/nhkworld/data/en/news/all.json | head -c 900; echo
echo; echo "=== nerv dns ==="
getent hosts unii-api.nerv.app || echo "NXDOMAIN unii-api.nerv.app"
getent hosts nerv.app || echo "NXDOMAIN nerv.app"
curl -sS -m 15 -o /dev/null -w 'nerv.app http=%{http_code}\n' https://nerv.app/ 2>&1 | tail -1
echo "=== gsj activefault real endpoints ==="
curl -sL -m 20 -A japanosint-collector https://gbank.gsj.jp/activefault/index_e_gmap.html \
  | grep -oiE '(href|src)="[^"]*"' | grep -iE 'json|cgi|xml|kml|php' | sort -u | head -20
