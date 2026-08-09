#!/usr/bin/env bash
echo "=== rferl body ==="
curl -sS -m 12 -A "japanosint-collector" "https://www.rferl.org/api/zrqiteuuir" | head -c 900
echo
echo "=== candidates ==="
while read -r id url; do
  [ -z "$id" ] && continue
  r=$(curl -sS -o /tmp/a6c.xml -w '%{http_code} %{size_download}' -L -m 15 \
        -H "Accept: application/rss+xml,*/*" -A "japanosint-collector" "$url" 2>&1 | tail -1)
  n=$(grep -c -i '<item' /tmp/a6c.xml 2>/dev/null)
  e=$(grep -c -i '<entry' /tmp/a6c.xml 2>/dev/null)
  printf '%-26s %-58s %s items=%s entries=%s\n' "$id" "$url" "$r" "$n" "$e"
done < "$1"
