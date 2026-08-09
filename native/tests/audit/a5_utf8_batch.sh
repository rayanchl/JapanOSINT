#!/usr/bin/env bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
for id in nhk-news-rss tass china-daily korea-herald elpais-port lemonde-une ansa-it \
          repubblica spiegel-intl rfi-fr clarin-ar g1-globo elmundo times-of-india \
          anadolu moscow-times-2 kyiv-post mainichi-en global-times scmp-china2 \
          deutsche-welle africanews dw-top japan-times; do
  printf '%-22s ' "$id"
  python3 tests/audit/a5_utf8.py "$id" 2>&1 | tr '\n' ' '
  echo
done
