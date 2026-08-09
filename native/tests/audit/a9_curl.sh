#!/usr/bin/env bash
# a9_curl.sh <urlsfile>   -- lines: id<TAB>url
while IFS=$'\t' read -r id url; do
  [ -z "${id:-}" ] && continue
  out=$(curl -sS -o /tmp/a9body -D /tmp/a9hdr -w '%{http_code} %{size_download} %{url_effective}' \
        --max-time 20 -H 'Accept: application/rss+xml,*/*' -H 'User-Agent: japanosint-collector' \
        "$url" 2>&1 | tail -1)
  ct=$(grep -i '^content-type:' /tmp/a9hdr | tail -1 | tr -d '\r')
  loc=$(grep -i '^location:' /tmp/a9hdr | tail -1 | tr -d '\r')
  head1=$(head -c 120 /tmp/a9body | tr '\n' ' ')
  echo "$id | $out | $ct | $loc | $head1"
done < "$1"
