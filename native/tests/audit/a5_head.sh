#!/usr/bin/env bash
UA='JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)'
for u in \
  https://www.blitzortung.org/en/live_lightning_maps.php \
  https://kakaku.com/ \
  https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/kenkou_iryou/shokuhin/syokuchu/index.html \
  https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/0000177182.html \
  https://www.tele.soumu.go.jp/giga-search/ \
  https://traininfo.jr-central.co.jp/zairaisen/index.html ; do
  printf '%-70s ' "${u:0:68}"
  curl -sS -I -m 20 -A "$UA" -o /dev/null -w 'HEAD=%{http_code} t=%{time_total}\n' "$u" 2>&1 | head -1
done
