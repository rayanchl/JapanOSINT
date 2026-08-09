#!/usr/bin/env bash
for u in https://www.freespot.com/gmap/13tokyo-e.xml \
         https://www.freespot.com/users/gmap/13tokyo-e.xml \
         https://www.freespot.com/gmap/xml/13tokyo-e.xml ; do
  printf '%-55s ' "$u"
  curl -sL -m 20 -o /tmp/a7x.xml -w 'http=%{http_code} sz=%{size_download} ct=%{content_type}\n' \
    -A japanosint-collector "$u"
done
echo "--- head of last good ---"
curl -sL -m 20 -A japanosint-collector https://www.freespot.com/gmap/13tokyo-e.xml -o /tmp/a7x.xml
head -c 1600 /tmp/a7x.xml; echo; echo "markers: $(grep -oc '<marker' /tmp/a7x.xml)"
