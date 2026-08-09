#!/usr/bin/env bash
# Is arXiv refusing our new User-Agent, or has the feed changed shape?
set -u
URL='https://rss.arxiv.org/rss/cs.AI'
NEW='JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; feed collector; contact via repo issues)'
OLD='japanosint-collector'
for ua in "$NEW" "$OLD" "curl/8"; do
  code=$(curl -s -o /tmp/ax.xml -w '%{http_code} %{size_download}' \
         -A "$ua" -H 'Accept: application/rss+xml,*/*' --max-time 25 "$URL")
  items=$(grep -c '<item' /tmp/ax.xml || true)
  echo "UA=[${ua:0:40}] -> $code  items=$items"
  head -c 160 /tmp/ax.xml | tr -d '\n'; echo; echo
done
