#!/usr/bin/env bash
grep -oE 'href="[^"]+"' /tmp/a7fs.html | sort -u | head -40
echo "=== try candidates ==="
for u in https://www.freespot.com/users/map_j.html \
         https://www.freespot.com/users/spot.html \
         https://www.freespot.com/users/list.html \
         https://www.freespot.com/users/e_01.html \
         https://www.freespot.com/users/01.html ; do
  printf '%-50s ' "$u"
  curl -sL -m 15 -o /tmp/a7c.html -w 'http=%{http_code} sz=%{size_download} ' -A japanosint-collector "$u"
  echo "tr=$(grep -c '<tr' /tmp/a7c.html) td=$(grep -c '<td' /tmp/a7c.html)"
done
