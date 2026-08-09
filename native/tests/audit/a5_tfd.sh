#!/usr/bin/env bash
for u in \
  https://www.tfd.metro.tokyo.lg.jp/lfe/saigai/ \
  https://www.tfd.metro.tokyo.lg.jp/lfe/saigai/index.html \
  https://www.tfd.metro.tokyo.lg.jp/saigai/saigai.html \
  https://www.tfd.metro.tokyo.lg.jp/lfe/topics/saigai.html \
  https://tokyo.mvsis.jp/ ; do
  code=$(curl -sSL -m 20 -o /tmp/tfd2.html -w '%{http_code}' "$u")
  echo "$code  trs=$(grep -c '<tr' /tmp/tfd2.html)  bytes=$(wc -c </tmp/tfd2.html)  $u"
done
echo "--- link discovery on lfe/ ---"
curl -sSL -m 20 https://www.tfd.metro.tokyo.lg.jp/lfe/ | grep -oiE 'href="[^"]*saigai[^"]*"' | sort -u | head
