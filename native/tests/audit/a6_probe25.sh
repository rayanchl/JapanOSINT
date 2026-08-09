#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_apis2.txt
: > "$OUT"
B="Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
p() { c=$(curl -sS -o /tmp/a6b.txt -w '%{http_code}' -m 30 -L -A "${3:-JapanOSINT/1.0}" "$2")
      echo "--- $1 [$c] $(wc -c < /tmp/a6b.txt)b" >> "$OUT"
      head -c 200 /tmp/a6b.txt | tr -d '\n' >> "$OUT"; echo >> "$OUT"; }
p stooq-browser "https://stooq.com/q/l/?s=aapl.us&f=sd2t2ohlcv&h&e=csv" "$B"
p stooq-nohdr   "https://stooq.com/q/l/?s=aapl.us&f=sd2t2ohlcv&e=csv" "$B"
p trongrid      "https://api.trongrid.io/v1/accounts/TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t"
p gitlab-u1     "https://gitlab.com/api/v4/users?username=gitlab-bot"
p gitlab-u2     "https://gitlab.com/api/v4/users?username=dosuken123"
p gists-defunkt "https://api.github.com/users/defunkt/gists?per_page=5"
p borme-1       "https://www.boe.es/borme/dias/2026/07/31/"
p borme-2       "https://boe.es/buscar/borme.php?campo%5B0%5D=BORME-C-A&dato%5B0%5D=Telefonica"
p borme-3       "https://www.boe.es/buscar/ayudas/borme_ayuda.php"
echo "--- SDN.XML head" >> "$OUT"
curl -sS -o /tmp/a6sdn.xml -w 'SDN.XML [%{http_code}] %{size_download}b\n' -m 120 \
  "https://sanctionslistservice.ofac.treas.gov/api/PublicationPreview/exports/SDN.XML" >> "$OUT"
grep -c -o "Digital Currency Address" /tmp/a6sdn.xml >> "$OUT" 2>&1
cat "$OUT"
