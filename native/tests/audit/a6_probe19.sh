#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_probe19.txt
: > "$OUT"
echo "### NRA radiation feeds" >> "$OUT"
for u in https://radioactivity.nra.go.jp/cont/json/dose_today.json \
         https://radioactivity.nra.go.jp/cont/json/MP/MP_today.json \
         https://www.kankyo-hoshano.go.jp/api/now_value.json \
         https://emdb.jaea.go.jp/emdb/api/v1/monitoring_post/latest.json ; do
  c=$(curl -sS -o /tmp/a6n.json -w '%{http_code}' -m 20 -L "$u" 2>&1 | tail -1)
  echo "--- $u [$c] $(wc -c < /tmp/a6n.json)b" >> "$OUT"
  head -c 300 /tmp/a6n.json >> "$OUT"; echo >> "$OUT"
done
echo "### radioactivity.nra.go.jp root" >> "$OUT"
curl -sS -o /tmp/a6nr.html -w 'root [%{http_code}] %{size_download}b\n' -m 20 -L https://radioactivity.nra.go.jp/ >> "$OUT" 2>&1
grep -o -E '[a-zA-Z0-9./_-]*\.(json|csv)' /tmp/a6nr.html | sort -u | head -20 >> "$OUT"
echo "### bluesky jetstream handshake" >> "$OUT"
curl -sS -i -N -m 10 -o /tmp/a6ws.txt -w 'ws [%{http_code}]\n' \
  -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Sec-WebSocket-Version: 13" \
  -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
  "https://jetstream2.us-west.bsky.network/subscribe?wantedCollections=app.bsky.feed.post" >> "$OUT" 2>&1
head -c 400 /tmp/a6ws.txt >> "$OUT"
cat "$OUT"
