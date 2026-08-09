#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'
echo "=== tradingeconomics stream json"
curl -sL --max-time 30 'https://tradingeconomics.com/ws/stream.ashx?start=0&size=3' -o /tmp/te.json -w '%{http_code} %{size_download}\n'
python3 -m json.tool /tmp/te.json | head -40
echo "=== kitco feed discovery"
curl -sL --max-time 30 -A "$UA" 'https://www.kitco.com/news' -o /tmp/k.html -w '%{http_code} %{size_download}\n'
grep -oiE '<link[^>]+(rss|atom|application/xml)[^>]*>' /tmp/k.html | head -10
grep -oiE 'href="[^"]*(rss|feed)[^"]*"' /tmp/k.html | sort -u | head -10
