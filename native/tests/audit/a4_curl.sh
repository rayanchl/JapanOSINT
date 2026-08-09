#!/bin/bash
# usage: a4_curl.sh urls.txt
while read -r u; do
  [ -z "$u" ] && continue
  code=$(curl -sS -o /tmp/a4c.$$ -w '%{http_code} %{size_download} %{url_effective}' -L --max-time 25 \
    -H 'Accept: application/rss+xml,*/*' -A 'japanosint-collector' "$u" 2>/tmp/a4e.$$)
  echo "--- $u"
  echo "    $code  err=$(head -c 200 /tmp/a4e.$$ | tr '\n' ' ')"
  echo "    head: $(head -c 300 /tmp/a4c.$$ | tr '\n' ' ')"
done < "$1"
rm -f /tmp/a4c.$$ /tmp/a4e.$$
