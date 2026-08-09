#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_ofac2.txt
curl -sS -L -o /tmp/a6sdn.csv -m 180 "https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN.CSV"
{
  echo "size=$(wc -c < /tmp/a6sdn.csv) lines=$(wc -l < /tmp/a6sdn.csv)"
  echo "=== first record with a digital currency address ==="
  grep -m1 -n 'Digital Currency Address' /tmp/a6sdn.csv | cut -c1-1200
  echo
  echo "=== a known XBT address record ==="
  grep -o -m1 'Digital Currency Address - XBT [A-Za-z0-9]*' /tmp/a6sdn.csv
} > "$OUT"
cat "$OUT"
