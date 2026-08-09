#!/usr/bin/env bash
echo "=== openplz at 1010"
curl -sS -m 20 -H 'Accept: application/json' "https://openplzapi.org/at/Localities?postalCode=1010&page=1&pageSize=20" | python3 -m json.tool | head -50
echo "=== openplz ch 8001"
curl -sS -m 20 -H 'Accept: application/json' "https://openplzapi.org/ch/Localities?postalCode=8001&page=1&pageSize=20" | python3 -c "import json,sys;d=json.load(sys.stdin);print(len(d));print(json.dumps(d[:3],ensure_ascii=False)[:600])"
