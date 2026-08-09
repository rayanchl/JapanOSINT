#!/usr/bin/env bash
# Start the server on a scratch DB and read /api/layers directly, so the camera
# count is verified from the API the UI actually calls — not inferred.
set -u
cd "$(dirname "$0")/../.." || exit 1
T=$(mktemp -d); DB="$T/t.db"
SECRET=layercheck
b64url() { openssl base64 -A | tr '+/' '-_' | tr -d '='; }
H=$(printf '%s' '{"alg":"HS256","typ":"JWT"}' | b64url)
P=$(printf '{"sub":"chk","aud":"authenticated","role":"authenticated","exp":%s}' \
      "$(( $(date +%s) + 3600 ))" | b64url)
SIG=$(printf '%s' "$H.$P" | openssl dgst -sha256 -hmac "$SECRET" -binary | b64url)
TOK="$H.$P.$SIG"

JO_DB="$DB" JO_SCHEMA=core/schema.sql SUPABASE_URL="" \
  SUPABASE_JWT_SECRET="$SECRET" PORT=4099 ./bin/japanosint --serve \
  >"$T/srv.log" 2>&1 &
SRV=$!
for _ in $(seq 1 60); do
  curl -s "http://127.0.0.1:4099/api/health" >/dev/null 2>&1 && break; sleep 0.5
done

curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:4099/api/layers" \
  > "$T/layers.json"
echo "bytes: $(wc -c <"$T/layers.json")"
python3 - "$T/layers.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
layers = d.get("layers", d) if isinstance(d, dict) else d
print("layers returned : %d" % len(layers))
tot = 0
for l in layers:
    n = len(l.get("sources") or [])
    tot += n
    if l.get("id") == "cameras":
        print("cameras layer   : %d sources" % n)
        print("  " + " ".join(sorted(s if isinstance(s, str) else s.get("id", "?")
                                     for s in l["sources"])))
print("total source refs across all layers: %d" % tot)
big = sorted(layers, key=lambda x: -len(x.get("sources") or []))[:8]
print("\nlargest layers:")
for l in big:
    print("  %-24s %3d" % (l.get("id"), len(l.get("sources") or [])))
PY
kill "$SRV" 2>/dev/null
rm -rf "$T"
