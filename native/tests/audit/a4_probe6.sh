#!/bin/bash
UA='Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36'
for u in 'https://whc.unesco.org/en/list/xml/' 'https://whc.unesco.org/en/list/rss/' \
         'https://www.kitco.com/rss/kitconews.xml' 'https://www.kitco.com/news/feed' 'https://feeds.feedburner.com/KitcoNews' \
         'https://kitco.com/rss/KitcoNews.xml' 'https://www.kitco.com/services/rss/kitconews.xml'; do
  echo "-- $u"
  curl -sL --max-time 25 -A "$UA" -H 'Accept: */*' "$u" -o /tmp/p.txt -w '   %{http_code} %{size_download} ct=%{content_type}\n'
  head -c 220 /tmp/p.txt | tr '\n' ' '; echo
done
