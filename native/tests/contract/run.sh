#!/usr/bin/env bash
# P7 contract-parity harness.
#
# Real prod auth is Supabase RS256/JWKS (SUPABASE_JWT_SECRET is empty in .env),
# so forged tokens can't be accepted in prod mode. For ROUTE-LOGIC parity we
# run BOTH servers under an identical "parity profile": HS256 with a known
# test secret and JWKS disabled (SUPABASE_URL=""). This isolates route/JSON
# parity from the prod auth mechanism (auth itself is covered by P3's
# 401/503 byte-parity tests).
#
# Usage: run.sh                # capture Node fixtures + diff C
#        run.sh capture        # only (re)capture Node fixtures
set -euo pipefail
ROOT=/Users/rayan/JapanOSINT
HERE="$ROOT/native/tests/contract"
SECRET=parity-secret

# Neutralize ambient credential env for BOTH servers. /api/status reports
# per-source `configured/set/missingVars` from process.env; whatever API keys
# happen to live in the developer's shell would otherwise make the captured
# Node fixture non-deterministic and unmatchable by a key-less C run. Same
# rationale as forcing SUPABASE_URL="" — the contract test isolates route
# logic, not the host's secret inventory. (List = apiCredentials.js vars.)
CRED_UNSET="-u AERODATABOX_KEY -u EDINET_API_KEY -u ESTAT_API_KEY -u ESTAT_APP_ID -u FACEBOOK_ACCESS_TOKEN -u FOFA_API_KEY -u GITHUB_TOKEN -u GOOGLE_MYMAPS_IDS -u GRAYHAT_API_KEY -u GREYNOISE_API_KEY -u HOTPEPPER_API_KEY -u MARINETRAFFIC_API_KEY -u MISSKEY_TOKEN -u MLIT_N02_GEOJSON_URL -u MLS_API_KEY -u ODPT_CHALLENGE_TOKEN -u ODPT_CONSUMER_KEY -u ODPT_TOKEN -u OPENCELLID_KEY -u OPENCHARGEMAP_KEY -u OPENSKY_CLIENT_ID -u OPENSKY_CLIENT_SECRET -u QUAKE_API_KEY -u RESAS_API_KEY -u SENTINELHUB_CLIENT_ID -u SENTINELHUB_CLIENT_SECRET -u SHODAN_API_KEY -u TWITTER_BEARER_TOKEN -u UMISHIRU_API_KEY -u USGS_M2M_TOKEN -u VESSELFINDER_API_KEY -u WIGLE_API_KEY -u WINDY_API_KEY"
ROUTES=(/api/status /api/sources /api/layers /api/intel/sources
        "/api/intel/items?limit=10")

mk_token() {
  node -e 'const c=require("crypto");const b=o=>Buffer.from(JSON.stringify(o)).toString("base64url");
  const h=b({alg:"HS256",typ:"JWT"}),p=b({sub:"parity",aud:"authenticated",role:"authenticated",exp:Math.floor(Date.now()/1e3)+86400});
  process.stdout.write(h+"."+p+"."+c.createHmac("sha256",process.argv[1]).update(h+"."+p).digest("base64url"))' "$SECRET"
}

# Free both ports before each run. A stale *prod* Node on :4071 (real API
# keys in its env) silently poisons the fixtures — captured /api/status would
# show configured creds the parity profile must never see. Hard-stop first.
free_ports() {
  for port in 4071 4072; do
    pids=$(lsof -ti "tcp:$port" 2>/dev/null || true)
    [ -n "$pids" ] && kill -9 $pids 2>/dev/null || true
  done
  pkill -f "$ROOT/server/src/index.js" 2>/dev/null || true
  sleep 1
}

start_node() {
  free_ports
  ( set -a; . "$ROOT/.env"; set +a; SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" \
    env $CRED_UNSET PORT=4071 node "$ROOT/server/src/index.js" ) >/tmp/jo_parity_node.log 2>&1 &
  echo $!
  for _ in $(seq 1 40); do curl -s http://127.0.0.1:4071/api/health >/dev/null 2>&1 && return; sleep 0.5; done
}
start_c() {
  lsof -ti tcp:4072 2>/dev/null | xargs kill -9 2>/dev/null || true
  SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" \
    env $CRED_UNSET PORT=4072 "$ROOT/native/bin/japanosint" --serve \
    >/tmp/jo_parity_c.log 2>&1 &
  echo $!
  for _ in $(seq 1 30); do curl -s http://127.0.0.1:4072/api/health >/dev/null 2>&1 && return; sleep 0.3; done
}

TOK=$(mk_token)
NPID=$(start_node)
for r in "${ROUTES[@]}"; do
  f=$(echo "$r" | sed 's/[/?=&]/_/g')
  curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:4071$r" > "$HERE/$f.node.json" || true
  echo "captured $r -> $f.node.json ($(wc -c <"$HERE/$f.node.json")B)"
done
kill "$NPID" 2>/dev/null || true

[ "${1:-}" = capture ] && exit 0

# Mask server-generated wall-clock fields that legitimately differ between the
# Node-capture run and the C run: the top-level "timestamp" (/health,/status)
# and the response-envelope "meta":{"fetched_at"} (/intel/*). These are
# generated at request time via new Date().toISOString() in BOTH servers, so
# they can never byte-match by construction. Masking is deliberately narrow —
# row-level data timestamps (fetched_at/published_at inside data items) carry
# no "meta":{ prefix and are NOT touched, so real payload parity still gates.
ISO='[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]*\.?[0-9]*Z'
norm() {
  sed -E -e "s/\"timestamp\":\"$ISO\"/\"timestamp\":\"<TS>\"/g" \
         -e "s/\"meta\":\{\"fetched_at\":\"$ISO\"/\"meta\":{\"fetched_at\":\"<TS>\"/g" \
         "$1"
}

CPID=$(start_c)
pass=0; tot=0
for r in "${ROUTES[@]}"; do
  f=$(echo "$r" | sed 's/[/?=&]/_/g'); tot=$((tot+1))
  curl -s -H "Authorization: Bearer $TOK" "http://127.0.0.1:4072$r" > "$HERE/$f.c.json" || true
  if diff -q <(norm "$HERE/$f.node.json") <(norm "$HERE/$f.c.json") >/dev/null 2>&1; then
    echo "PARITY  $r"; pass=$((pass+1))
  else
    echo "DIFF    $r (node $(wc -c <"$HERE/$f.node.json")B vs c $(wc -c <"$HERE/$f.c.json")B)"
  fi
done
kill "$CPID" 2>/dev/null || true
echo "CONTRACT PARITY: $pass/$tot"
