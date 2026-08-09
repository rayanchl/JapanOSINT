#!/usr/bin/env bash
U=$(date +%s); F=$((U-604800))
curl -sS -m 40 -H 'Accept: application/json' \
  "https://api.ioda.inetintel.cc.gatech.edu/v2/signals/raw/country/JP?from=$F&until=$U&datasource=bgp" \
  > /tmp/a5_ioda.json
echo "bytes: $(wc -c < /tmp/a5_ioda.json)"
python3 - <<'EOF'
import json
d = json.load(open('/tmp/a5_ioda.json'))
print("top keys:", list(d.keys()))
dd = d.get("data")
print("data type:", type(dd).__name__, "len:", len(dd) if dd is not None else None)
x = dd[0]
print("elem0 type:", type(x).__name__)
if isinstance(x, list):
    print("  inner len:", len(x), "inner0 type:", type(x[0]).__name__)
    x = x[0]
if isinstance(x, dict):
    for k, v in x.items():
        s = repr(v)
        print("   %-22s %s" % (k, s[:110]))
EOF
