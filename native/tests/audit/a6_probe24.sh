#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_apis.txt
: > "$OUT"
U="User-Agent: JapanOSINT/1.0 (osint@example.org)"
p() { # label url
  c=$(curl -sS -o /tmp/a6a.txt -w '%{http_code}' -m 25 -L -H "Accept: application/json" -H "$U" "$2")
  echo "--- $1 [$c] $(wc -c < /tmp/a6a.txt)b" >> "$OUT"
  head -c 220 /tmp/a6a.txt | tr -d '\n' >> "$OUT"; echo >> "$OUT"
}
p ofac-sdn        "https://www.treasury.gov/ofac/downloads/sdn_advanced.xml"
p ofac-sdn-alt    "https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN_ADVANCED.XML"
p tronscan        "https://apilist.tronscanapi.com/api/accountv2?address=TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t"
p blockchair      "https://api.blockchair.com/bitcoin/dashboards/address/1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"
p stooq           "https://stooq.com/q/l/?s=aapl.us&f=sd2t2ohlcv&h&e=csv"
p stooq2          "https://stooq.pl/q/l/?s=aapl.us&f=sd2t2ohlcv&h&e=csv"
p worldbank       "https://api.worldbank.org/v2/country/all/indicator/JP?format=json&per_page=50&mrv=1"
p worldbank2      "https://api.worldbank.org/v2/country/all/indicator/NY.GDP.MKTP.CD?format=json&per_page=50&mrv=1"
p gitlab-user     "https://gitlab.com/api/v4/users?username=torvalds"
p github-gists    "https://api.github.com/users/tokyo/gists?per_page=20"
p aleph           "https://aleph.occrp.org/api/2/entities?q=Gazprom&limit=15"
p borme           "https://www.boe.es/buscar/borme.php?campo%5B0%5D=BORME-C-A&dato%5B0%5D=Telefonica"
cat "$OUT"
