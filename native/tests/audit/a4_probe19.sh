#!/bin/bash
echo "=== onionoo details country=jp flag=Exit"
curl -sL --max-time 30 -H 'Accept: application/json' 'https://onionoo.torproject.org/details?country=jp&flag=Exit' -o /tmp/oo.json -w '%{http_code} %{size_download}\n'
python3 -c "
import json
d=json.load(open('/tmp/oo.json'))
r=d.get('relays',[])
print('relays:',len(r))
withgeo=[x for x in r if x.get('latitude') is not None]
print('with lat:',len(withgeo))
if r: print({k:r[0].get(k) for k in ('nickname','country','latitude','longitude','as_name','city_name')})
"
echo "=== onionoo lowercase jp variant / search"
curl -sL --max-time 30 -H 'Accept: application/json' 'https://onionoo.torproject.org/details?search=country:jp&flag=Exit' -o /tmp/oo2.json -w '%{http_code} %{size_download}\n'
python3 -c "
import json;d=json.load(open('/tmp/oo2.json'));r=d.get('relays',[]);print('relays:',len(r));print('with lat:',len([x for x in r if x.get('latitude') is not None]))"
echo "=== reddit"
curl -sL --max-time 25 -A 'japanosint-collector' 'https://www.reddit.com/r/japan/new.json?limit=25' -o /tmp/rd.json -w '%{http_code} %{size_download}\n'
head -c 200 /tmp/rd.json; echo
