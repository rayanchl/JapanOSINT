#!/usr/bin/env bash
OUT=/mnt/c/Users/rayan/sources/repos/OSINTsaas/native/tests/audit/a6_o_note.txt
curl -sS -m 20 -H 'user-agent: JapanOSINT/1.0' -H 'accept: application/json' \
  "https://note.com/api/v3/searches?context=user&q=OSINT&size=20" -o /tmp/a6n3.json
python3 - <<'PY' > "$OUT"
import json
d=json.load(open('/tmp/a6n3.json'))
us=d["data"]["users"]["contents"]
print("users:",len(us))
print("keys:",list(us[0].keys()))
print(json.dumps(us[0],ensure_ascii=False)[:1200])
print("users obj keys:",list(d["data"]["users"].keys()))
PY
cat "$OUT"
