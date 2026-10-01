#!/usr/bin/env bash
# Node-vs-C latency comparison (simple curl -w table). With server/ deleted
# this degrades to a C-only latency table — see NODE_OK below.
#
# Reuses the contract harness's parity profile: both backends under HS256
# with a known secret, JWKS disabled (SUPABASE_URL=""), ambient API-key env
# neutralized. Node on :4071, C on :4072.
#
# THE DATABASE.  This harness is destructive by design: it provisions a bench
# tenant, rewrites its plan, and deletes the lot again from a `trap cleanup
# EXIT` that also fires on Ctrl-C. For as long as its ROOT derivation was
# broken it never reached any of that; fixing the path made every one of those
# statements reachable — against $ROOT/data/japanmap.db, 7+ GB of collected
# intel, on a box where the operator's own server is very likely running off
# the same file. So: it runs against a SCRATCH database, and it refuses to
# start against the real one.
#
# Usage:
#   run.sh                  # default light route set, scratch DB
#   run.sh --with-sweeps    # also benchmark the heavy /api/data sweep layers
#   N=10 run.sh             # runs per route (default 6; first is warmup)
#   JO_BENCH_DB=/path/copy.db run.sh     # bench a COPY of a populated DB
#   JO_BENCH_ALLOW_REAL_DB=1 run.sh      # explicit, deliberate, destructive
set -euo pipefail
# Derive the repo root from this script's own location. It was hardcoded to
# /Users/rayan/JapanOSINT — the absolute path of the machine the harness was
# written on — so the suite could not run in any other checkout. The prior
# audit report attributed this to tests/contract/run.sh; that one was fixed and
# this one was missed, so it kept failing before its first sample. Same
# resolution as tests/contract/run.sh; JO_REPO_ROOT still overrides.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${JO_REPO_ROOT:-$(cd "$HERE/../../.." && pwd)}"
SECRET=parity-secret
# The Node backend (server/) was deleted on 2026-05-17. Where it is absent this
# becomes a C-only latency table rather than half a comparison against a server
# that cannot start — the NODE column reads "-" and nothing is invented.
NODE_OK=0
[ -f "$ROOT/server/src/index.js" ] && command -v node >/dev/null 2>&1 && NODE_OK=1
N="${N:-6}"                       # samples/route (run 1 discarded as warmup)
WITH_SWEEPS=0
[ "${1:-}" = "--with-sweeps" ] && WITH_SWEEPS=1

# ---- database safety -----------------------------------------------------
# Default target: a scratch file the C server creates from core/schema.sql on
# first open (core/db.c: JO_DB, SQLITE_OPEN_CREATE). Empty, so the numbers are
# schema-only latency; point JO_BENCH_DB at a *copy* of a populated database
# when you want representative ones. Never at the live database itself.
REAL_DB="$ROOT/data/japanmap.db"
OWN_SCRATCH_DB=0
if [ -n "${JO_BENCH_DB:-}" ]; then
  BENCH_DB="$JO_BENCH_DB"
elif [ "${JO_BENCH_ALLOW_REAL_DB:-0}" = "1" ]; then
  BENCH_DB="$REAL_DB"           # asked for by name; the guard below lets it by
else
  BENCH_DB="${TMPDIR:-/tmp}/jo_bench_$$.db"
  OWN_SCRATCH_DB=1
fi

abspath() {  # normalise without requiring GNU realpath (macOS host)
  local d b
  d=$(dirname -- "$1"); b=$(basename -- "$1")
  d=$(cd "$d" 2>/dev/null && pwd) || { printf '%s' "$1"; return; }
  printf '%s/%s' "$d" "$b"
}

BENCH_DB_ABS=$(abspath "$BENCH_DB")
REAL_DB_ABS=$(abspath "$REAL_DB")
DATA_DIR_ABS=$(abspath "$ROOT/data")

if [ "${JO_BENCH_ALLOW_REAL_DB:-0}" != "1" ] \
   && { [ "$BENCH_DB_ABS" = "$REAL_DB_ABS" ] \
        || case "$BENCH_DB_ABS" in "$DATA_DIR_ABS"/*) true ;; *) false ;; esac; }
then
  cat >&2 <<EOF
bench/run.sh: refusing to run against $BENCH_DB_ABS

This harness DELETEs from memberships/tenants/users and UPDATEs tenants.plan,
and it does so from an EXIT trap that fires on Ctrl-C too. $DATA_DIR_ABS holds
the collected intel store ($REAL_DB_ABS), not fixtures.

  - default (no env)              -> scratch DB under \${TMPDIR:-/tmp}
  - JO_BENCH_DB=/path/copy.db     -> bench a copy you made yourself
  - JO_BENCH_ALLOW_REAL_DB=1      -> yes, really, do it to the live database
EOF
  exit 2
fi
# Every child below reads this; it also overrides any ambient JO_DB (and any
# JO_DB inside .env, which the Node subshell sources).
export JO_DB="$BENCH_DB_ABS"

CRED_UNSET="-u AERODATABOX_KEY -u EDINET_API_KEY -u ESTAT_API_KEY -u ESTAT_APP_ID -u FACEBOOK_ACCESS_TOKEN -u FOFA_API_KEY -u GITHUB_TOKEN -u GOOGLE_MYMAPS_IDS -u GRAYHAT_API_KEY -u GREYNOISE_API_KEY -u HOTPEPPER_API_KEY -u MARINETRAFFIC_API_KEY -u MISSKEY_TOKEN -u MLIT_N02_GEOJSON_URL -u MLS_API_KEY -u ODPT_CHALLENGE_TOKEN -u ODPT_CONSUMER_KEY -u ODPT_TOKEN -u OPENCELLID_KEY -u OPENCHARGEMAP_KEY -u OPENSKY_CLIENT_ID -u OPENSKY_CLIENT_SECRET -u QUAKE_API_KEY -u RESAS_API_KEY -u SENTINELHUB_CLIENT_ID -u SENTINELHUB_CLIENT_SECRET -u SHODAN_API_KEY -u TWITTER_BEARER_TOKEN -u UMISHIRU_API_KEY -u USGS_M2M_TOKEN -u VESSELFINDER_API_KEY -u WIGLE_API_KEY -u WINDY_API_KEY"

# Operator allowlist so /api/db + /api/admin return data (not 403) for the
# forged identity, exercising those code paths on both backends.
OP_EMAIL=bench@local

ROUTES=(
  /api/health
  /api/sources
  /api/status
  /api/intel/sources
  "/api/intel/items?limit=50"
  /api/entities/stats
  /api/me
  /api/audit
  /api/alerts
  /api/db/tables
  /api/admin/maintenance
)
if [ "$WITH_SWEEPS" = 1 ]; then
  ROUTES+=(/api/data/unified-trains /api/data/unified-subways
           /api/data/unified-buses /api/data/cameras /api/data/unified-stations)
fi

# openssl fallback so the bench still runs on a host with no Node (the normal
# case now that server/ is gone) — same construction as tests/contract/run.sh.
b64url() { openssl base64 -A | tr '+/' '-_' | tr -d '='; }
mk_token() {
  if command -v node >/dev/null 2>&1; then
    node -e 'const c=require("crypto");const b=o=>Buffer.from(JSON.stringify(o)).toString("base64url");
    const h=b({alg:"HS256",typ:"JWT"}),p=b({sub:"bench",email:"bench@local",aud:"authenticated",role:"authenticated",exp:Math.floor(Date.now()/1e3)+86400});
    process.stdout.write(h+"."+p+"."+c.createHmac("sha256",process.argv[1]).update(h+"."+p).digest("base64url"))' "$SECRET"
    return
  fi
  local h p sig
  h=$(printf '%s' '{"alg":"HS256","typ":"JWT"}' | b64url)
  p=$(printf '{"sub":"bench","email":"bench@local","aud":"authenticated","role":"authenticated","exp":%s}' \
        "$(( $(date +%s) + 86400 ))" | b64url)
  sig=$(printf '%s' "$h.$p" | openssl dgst -sha256 -hmac "$SECRET" -binary | b64url)
  printf '%s.%s.%s' "$h" "$p" "$sig"
}

# Only the two bench ports. The blanket `pkill -9 -f .../bin/japanosint` that
# used to live here matched EVERY instance of the binary on the box, including
# the operator's own long-running --serve on :3000 — a benchmark has no
# business SIGKILLing a server it did not start.
free_ports() {
  for port in 4071 4072; do
    pids=$(lsof -ti "tcp:$port" 2>/dev/null || true)
    [ -n "$pids" ] && kill -9 $pids 2>/dev/null || true
  done
  sleep 1
}

NPID=""; CPID=""
cleanup() {
  [ -n "$CPID" ] && kill -9 "$CPID" 2>/dev/null || true
  [ -n "$NPID" ] && kill -9 "$NPID" 2>/dev/null || true
  free_ports
  # Drop the auto-provisioned bench tenant/user (first /api/me call creates it).
  # $JO_DB, never $ROOT/data/japanmap.db — see "database safety" above.
  sqlite3 "$JO_DB" \
    "DELETE FROM memberships WHERE user_id IN (SELECT id FROM users WHERE supabase_user_id='bench');
     DELETE FROM tenants WHERE id IN (SELECT tenant_id FROM memberships WHERE user_id IN (SELECT id FROM users WHERE supabase_user_id='bench'));
     DELETE FROM tenants WHERE name LIKE 'bench@local%';
     DELETE FROM users WHERE supabase_user_id='bench';" 2>/dev/null || true
  # A scratch DB this script created for itself goes away with it. One the
  # operator named (JO_BENCH_DB) is theirs and is left alone.
  if [ "$OWN_SCRATCH_DB" = 1 ]; then
    rm -f "$JO_DB" "$JO_DB-wal" "$JO_DB-shm"
  fi
}
trap cleanup EXIT

free_ports
echo "db: $JO_DB${JO_BENCH_DB:+ (JO_BENCH_DB)}"
if [ "$NODE_OK" = 1 ]; then
  # JO_DB restated after the .env source: .env may carry its own JO_DB and
  # `set -a; . .env` would otherwise win inside this subshell.
  ( set -a; . "$ROOT/.env"; set +a; SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" \
    JO_DB="$JO_DB" DB_PATH="$JO_DB" \
    PLATFORM_OPERATOR_EMAILS="$OP_EMAIL" env $CRED_UNSET PORT=4071 \
    node "$ROOT/server/src/index.js" ) >/tmp/jo_bench_node.log 2>&1 &
  NPID=$!
else
  echo "note: $ROOT/server/src/index.js is gone (Node backend removed 2026-05-17)"
  echo "      — running C-only; the NODE column will read '-'."
fi
SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" PLATFORM_OPERATOR_EMAILS="$OP_EMAIL" \
  JO_DB="$JO_DB" env $CRED_UNSET PORT=4072 "$ROOT/native/bin/japanosint" --serve \
  >/tmp/jo_bench_c.log 2>&1 &
CPID=$!

for _ in $(seq 1 60); do
  if [ "$NODE_OK" = 1 ]; then
    curl -s -o /dev/null http://127.0.0.1:4071/api/health 2>/dev/null \
      && curl -s -o /dev/null http://127.0.0.1:4072/api/health 2>/dev/null && break
  else
    curl -s -o /dev/null http://127.0.0.1:4072/api/health 2>/dev/null && break
  fi
  sleep 0.5
done
TOK=$(mk_token)

# Provision the bench user/tenant (first authed call), then upgrade it to
# the `enterprise` plan so Node's token-bucket rate limiter (free plan trips
# 429 under N rapid samples and would make the comparison meaningless —
# PLAN_MULTIPLIER.enterprise === Infinity → middleware/rateLimit.js returns
# next() with no cap). C has no rate limiter, so both now do real work.
if [ "$NODE_OK" = 1 ]; then
  curl -s -o /dev/null -H "Authorization: Bearer $TOK" http://127.0.0.1:4071/api/me || true
fi
curl -s -o /dev/null -H "Authorization: Bearer $TOK" http://127.0.0.1:4072/api/me || true
sqlite3 "$JO_DB" \
  "UPDATE tenants SET plan='enterprise' WHERE id IN
     (SELECT m.tenant_id FROM memberships m
        JOIN users u ON u.id=m.user_id WHERE u.supabase_user_id='bench');" 2>/dev/null || true

# median of stdin numbers (bash + sort)
med() { sort -n | awk '{a[NR]=$1} END{ if(NR==0){print "NA"} else if(NR%2){print a[(NR+1)/2]} else {print (a[NR/2]+a[NR/2+1])/2} }'; }

samp() {  # $1=base $2=route -> "median_s size httpcode"
  local base=$1 r=$2 i out code size t times=()
  for i in $(seq 1 "$N"); do
    out=$(curl -s -o /dev/null -m 30 -w "%{time_total} %{size_download} %{http_code}" \
          -H "Authorization: Bearer $TOK" "$base$r" 2>/dev/null || echo "30 0 000")
    [ "$i" = 1 ] && { code=${out##* }; size=$(echo "$out"|awk '{print $2}'); continue; }  # warmup
    times+=("$(echo "$out" | awk '{print $1}')")
    code=${out##* }; size=$(echo "$out" | awk '{print $2}')
  done
  local m; m=$(printf '%s\n' "${times[@]}" | med)
  echo "$m $size $code"
}

printf '%-34s %10s %10s %9s %12s %s\n' ROUTE NODE_med C_med "N/C" BYTES "HTTP(n/c)"
printf '%-34s %10s %10s %9s %12s %s\n' "-----" "-------" "------" "----" "--------" "----"
for r in "${ROUTES[@]}"; do
  if [ "$NODE_OK" = 1 ]; then
    read -r nmed nsize ncode <<<"$(samp http://127.0.0.1:4071 "$r")"
  else
    # No Node backend to sample. Emit nothing rather than a plausible-looking
    # zero: a fabricated baseline is worse than an absent one.
    nmed="-"; nsize="-"; ncode="-"
  fi
  read -r cmed csize ccode <<<"$(samp http://127.0.0.1:4072 "$r")"
  spd=$(awk -v n="$nmed" -v c="$cmed" 'BEGIN{ if(c>0 && n!="NA" && n!="-" && c!="NA") printf "%.2fx", n/c; else print "-" }')
  printf '%-34s %10s %9ss %9s %12s %s/%s\n' "$r" "$nmed" "$cmed" "$spd" "${nsize}|${csize}" "$ncode" "$ccode"
done
echo
if [ "$NODE_OK" = 1 ]; then
  echo "N=$N samples/route (1 warmup dropped). Node :4071  C :4072  db=$JO_DB."
else
  echo "N=$N samples/route (1 warmup dropped). C :4072 only — no Node backend exists."
  echo "db=$JO_DB"
fi
if [ "$OWN_SCRATCH_DB" = 1 ]; then
  echo "NOTE: that DB was an empty scratch file — these are schema-only"
  echo "      latencies, not row-count latencies. For representative numbers,"
  echo "      copy a populated database and pass JO_BENCH_DB=/path/to/copy.db."
fi
echo "Sizes node|c; differing sizes => shape divergence to investigate (not just speed)."
