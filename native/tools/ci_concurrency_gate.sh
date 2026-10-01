#!/bin/bash
# ci_concurrency_gate.sh — fail if a slow route can stop the whole server.
#
# WHY THIS IS A GATE AND NOT A NOTE. On 2026-09-07 one cold /api/data/castles
# request froze /api/health for over 20 s; on 2026-09-11 three concurrent
# /api/status calls did it for 24.8 s. Both were found by a human running a
# benchmark by hand, months apart, and both had been true since the code was
# written. The property is cheap to check and expensive to lose, so it is
# checked on every push: while a slow route is in flight, /api/health must
# still answer promptly.
#
# It needs no network. The server runs against a fresh schema-applied DB with
# the scheduler staggered out of the way, so the only work in flight is the
# request under test. On an empty database /api/status still serialises every
# registered source (16k+ rows), which is the expensive half.
#
#   native/tools/ci_concurrency_gate.sh [binary] [max_health_seconds]
#
# Defaults: ./bin/japanosint, 1.0 s. The measured post-fix figure on a laptop
# is ~0.001 s, and a loaded CI runner is not 1000x slower than that — a
# failure here means the route went back on the event loop, not that the box
# was busy.
set -u
BIN=${1:-./bin/japanosint}
MAX=${2:-1.0}
PORT=${PORT:-4077}
W=$(mktemp -d)
trap 'kill %1 2>/dev/null; rm -rf "$W"' EXIT

[ -x "$BIN" ] || { echo "gate: $BIN missing or not executable"; exit 1; }
command -v python3 >/dev/null || { echo "gate: python3 required (mints the test JWT)"; exit 1; }
command -v curl    >/dev/null || { echo "gate: curl required"; exit 1; }

SECRET=ci-concurrency-gate
python3 - "$W" "$SECRET" <<'PY'
import base64, hashlib, hmac, json, sys, time
w, sec = sys.argv[1], sys.argv[2].encode()
b64 = lambda b: base64.urlsafe_b64encode(b).rstrip(b"=")
h = b64(json.dumps({"alg": "HS256", "typ": "JWT"}, separators=(',', ':')).encode())
now = int(time.time())
p = b64(json.dumps({"sub": "ci", "aud": "authenticated", "role": "authenticated",
                    "iss": "https://ci.local/auth/v1", "iat": now, "exp": now + 3600},
                   separators=(',', ':')).encode())
open(w + "/token", "wb").write(h + b"." + p + b"." + b64(hmac.new(sec, h + b"." + p, hashlib.sha256).digest()))
PY
T=$(cat "$W/token")

# A huge stagger keeps every collector's first run in the future, so nothing
# reaches the network during the gate.
JO_DB="$W/gate.db" SUPABASE_JWT_SECRET="$SECRET" JO_JWT_ISS=https://ci.local/auth/v1 \
JO_BIND=127.0.0.1 PORT=$PORT JO_SCHED_WORKERS=1 JO_SCHED_STAGGER_SEC=100000 \
JO_FLEET_CACHE_SEC=0 \
"$BIN" --serve > "$W/server.log" 2>&1 &

for _ in $(seq 1 60); do
  curl -sf --max-time 2 "http://127.0.0.1:$PORT/api/health" >/dev/null 2>&1 && break
  sleep 1
done
curl -sf --max-time 2 "http://127.0.0.1:$PORT/api/health" >/dev/null || {
  echo "gate: server never became healthy"; tail -20 "$W/server.log"; exit 1; }

# The cache is disabled above (JO_FLEET_CACHE_SEC=0) on purpose: a cached hit
# would pass this gate without proving the build ran off the loop.
LOADPIDS=""
for _ in 1 2 3; do
  curl -s --max-time 120 -o /dev/null -H "Authorization: Bearer $T" \
    "http://127.0.0.1:$PORT/api/status" &
  LOADPIDS="$LOADPIDS $!"
done

: > "$W/probe"
END=$(( $(date +%s) + 25 ))
while [ "$(date +%s)" -lt "$END" ]; do
  curl -s --max-time 30 -o /dev/null -w '%{time_total}\n' \
    "http://127.0.0.1:$PORT/api/health" >> "$W/probe" 2>/dev/null || echo 30 >> "$W/probe"
  sleep 0.1
done
# Only the load requests — a bare `wait` would also wait for the server, which
# never exits, and the gate would hang forever instead of failing.
# shellcheck disable=SC2086
wait $LOADPIDS 2>/dev/null || true

N=$(wc -l < "$W/probe")
WORST=$(sort -n "$W/probe" | tail -1)
echo "gate: /api/health probed $N times while 3 x /api/status were in flight"
echo "gate: worst health latency ${WORST}s (limit ${MAX}s)"
awk -v w="$WORST" -v m="$MAX" 'BEGIN { exit !(w+0 > m+0) }' && {
  echo
  echo "FAIL: a slow route is blocking the event loop again."
  echo "      /api/status must build on a worker (see core/httpd.c fleet_serve"
  echo "      and docs/concurrency-plan-2026-09-11.md)."
  exit 1
}
echo "gate: ok"
