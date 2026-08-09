#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_ofac.txt
: > "$OUT"
for u in https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN.XML \
         https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN.CSV \
         https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/CONS_ADVANCED.XML ; do
  curl -sS -L -o /tmp/a6o.dat -w "$u [%{http_code}] %{size_download}b\n" -m 180 "$u" >> "$OUT"
  echo "  DigitalCurrency hits: $(grep -c -o 'Digital Currency Address' /tmp/a6o.dat)" >> "$OUT"
  echo "  sample: $(grep -o -m1 'Digital Currency Address[^<,]*' /tmp/a6o.dat | head -1)" >> "$OUT"
done
cat "$OUT"
