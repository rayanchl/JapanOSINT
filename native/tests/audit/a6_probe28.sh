#!/usr/bin/env bash
curl -sS -m 25 "https://api.trongrid.io/v1/accounts/TR7NHqjeKQxGTCi8q8ZY4pL8otSzgjLj6t" -o /tmp/a6tg.json
python3 - <<'PY'
import json
d=json.load(open('/tmp/a6tg.json'))
print("top:",list(d.keys()), "success=",d.get("success"))
a=d["data"][0]
for k,v in a.items():
    s=json.dumps(v,ensure_ascii=False)
    print("  %-26s %s" % (k, s[:120]))
PY
