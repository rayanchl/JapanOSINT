#!/usr/bin/env bash
curl -sS -m 25 -H 'Accept: application/json' \
  'https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-domain?domain=mufg.jp' \
  -o /tmp/a6hr.json
wc -c /tmp/a6hr.json
python3 - <<'PY'
import json
d = json.load(open('/tmp/a6hr.json'))
print("top-level type:", type(d).__name__)
if isinstance(d, dict):
    for k, v in d.items():
        if isinstance(v, (list, dict)):
            print("  %-28s %s len=%d  sample=%s" % (k, type(v).__name__, len(v), json.dumps(v[:1] if isinstance(v, list) else list(v)[:4], ensure_ascii=False)[:220]))
        else:
            print("  %-28s %r" % (k, v))
PY
