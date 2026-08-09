#!/bin/bash
for u in \
 'https://news.google.com/rss/search?q=PLA%20incursion&hl=zh-TW&gl=TW&ceid=TW:zh' \
 'https://news.google.com/rss/search?q=%E5%85%B1%E6%A9%9F%E6%93%BE%E5%8F%B0&hl=zh-TW&gl=TW&ceid=TW:zh' \
 'https://news.google.com/rss/search?q=%E8%A7%A3%E6%94%BE%E8%BB%8D%20%E6%93%BE%E5%8F%B0&hl=zh-TW&gl=TW&ceid=TW:zh' \
 'https://news.google.com/rss/search?q=Arctic%20militarization&hl=no&gl=NO&ceid=NO:no' \
 'https://news.google.com/rss/search?q=milit%C3%A6r%20opprustning%20Arktis&hl=no&gl=NO&ceid=NO:no' \
 'https://news.google.com/rss/search?q=Arktis%20milit%C3%A6r&hl=no&gl=NO&ceid=NO:no' ; do
  c=$(curl -sL --max-time 25 "$u" -o /tmp/gn.xml -w '%{http_code}')
  echo "$c items=$(grep -c '<item>' /tmp/gn.xml)  $u"
done
