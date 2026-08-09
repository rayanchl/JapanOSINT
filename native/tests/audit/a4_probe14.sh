#!/bin/bash
curl -sL --max-time 30 'https://www.seismicportal.eu/fdsnws/event/1/query?limit=3&format=json&orderby=time' -o /tmp/sp.json -w '%{http_code} %{size_download}\n'
python3 -m json.tool /tmp/sp.json | head -60
echo "=== iceland bbox"
curl -sL --max-time 30 'https://www.seismicportal.eu/fdsnws/event/1/query?limit=5&format=json&orderby=time&minlat=62.5&maxlat=67.5&minlon=-25.5&maxlon=-12.5' -o /tmp/sp2.json -w '%{http_code} %{size_download}\n'
python3 -c "
import json;d=json.load(open('/tmp/sp2.json'));print(len(d['features']));print(json.dumps(d['features'][0],indent=1) if d['features'] else '')"
