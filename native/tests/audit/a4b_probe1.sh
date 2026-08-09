#!/bin/bash
cd /mnt/c/Users/rayan/sources/repos/OSINTsaas/native || exit 1
echo "=== CMR AU_Land first entry keys"
curl -s --max-time 40 "https://cmr.earthdata.nasa.gov/search/granules.json?short_name=AU_Land&bounding_box=122.0,24.0,146.0,46.0&sort_key=-start_date&page_size=2" \
 | python3 -c "import sys,json; d=json.load(sys.stdin); e=d['feed']['entry']; print(len(e)); print(json.dumps(e[0], indent=1)[:3000])"
echo
echo "=== onionoo JP exit relays"
curl -s --max-time 40 "https://onionoo.torproject.org/details?country=jp&flag=Exit&type=relay" \
 | python3 -c "import sys,json; d=json.load(sys.stdin); r=d.get('relays',[]); print('relays',len(r)); print('with latlon',sum(1 for x in r if x.get('latitude') is not None)); print(json.dumps(r[0],indent=1)[:1500] if r else '')"
