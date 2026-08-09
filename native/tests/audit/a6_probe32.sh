#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_jma.txt
{
echo "--- targetTc.json"
curl -sS -m 20 -w '\n[%{http_code}] %{size_download}b\n' "https://www.jma.go.jp/bosai/typhoon/data/targetTc.json"
for u in https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-22/N05-22.geojson \
         https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-21/N05-21.geojson \
         https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N05-v1_3.html ; do
  echo "--- $u"
  curl -sS -o /tmp/a6k.dat -w '[%{http_code}] %{size_download}b\n' -m 40 -L "$u"
  head -c 150 /tmp/a6k.dat; echo
done
} > "$OUT"
cat "$OUT"
