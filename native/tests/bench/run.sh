#!/usr/bin/env bash
# Node-vs-C latency comparison (simple curl -w table).
#
# Reuses the contract harness's parity profile: both backends under HS256
# with a known secret, JWKS disabled (SUPABASE_URL=""), ambient API-key env
# neutralized, same server/data/japanmap.db. Node on :4071, C on :4072.
#
# Usage:
#   run.sh                  # default light route set
#   run.sh --with-sweeps    # also benchmark the heavy /api/data sweep layers
#   N=10 run.sh             # runs per route (default 6; first is warmup)
set -euo pipefail
ROOT=/Users/rayan/JapanOSINT
SECRET=parity-secret
N="${N:-6}"                       # samples/route (run 1 discarded as warmup)
WITH_SWEEPS=0
[ "${1:-}" = "--with-sweeps" ] && WITH_SWEEPS=1

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

mk_token() {
  node -e 'const c=require("crypto");const b=o=>Buffer.from(JSON.stringify(o)).toString("base64url");
  const h=b({alg:"HS256",typ:"JWT"}),p=b({sub:"bench",email:"bench@local",aud:"authenticated",role:"authenticated",exp:Math.floor(Date.now()/1e3)+86400});
  process.stdout.write(h+"."+p+"."+c.createHmac("sha256",process.argv[1]).update(h+"."+p).digest("base64url"))' "$SECRET"
}

free_ports() {
  for port in 4071 4072; do
    pids=$(lsof -ti "tcp:$port" 2>/dev/null || true)
    [ -n "$pids" ] && kill -9 $pids 2>/dev/null || true
  done
  pkill -f "$ROOT/server/src/index.js" 2>/dev/null || true
  pkill -9 -f "$ROOT/native/bin/japanosint" 2>/dev/null || true
  sleep 1
}

NPID=""; CPID=""
cleanup() {
  [ -n "$CPID" ] && kill -9 "$CPID" 2>/dev/null || true
  [ -n "$NPID" ] && kill -9 "$NPID" 2>/dev/null || true
  free_ports
  # Drop the auto-provisioned bench tenant/user (first /api/me call creates it).
  sqlite3 "$ROOT/server/data/japanmap.db" \
    "DELETE FROM memberships WHERE user_id IN (SELECT id FROM users WHERE supabase_user_id='bench');
     DELETE FROM tenants WHERE id IN (SELECT tenant_id FROM memberships WHERE user_id IN (SELECT id FROM users WHERE supabase_user_id='bench'));
     DELETE FROM tenants WHERE name LIKE 'bench@local%';
     DELETE FROM users WHERE supabase_user_id='bench';" 2>/dev/null || true
}
trap cleanup EXIT

free_ports
( set -a; . "$ROOT/.env"; set +a; SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" \
  PLATFORM_OPERATOR_EMAILS="$OP_EMAIL" env $CRED_UNSET PORT=4071 \
  node "$ROOT/server/src/index.js" ) >/tmp/jo_bench_node.log 2>&1 &
NPID=$!
SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" PLATFORM_OPERATOR_EMAILS="$OP_EMAIL" \
  env $CRED_UNSET PORT=4072 "$ROOT/native/bin/japanosint" --serve \
  >/tmp/jo_bench_c.log 2>&1 &
CPID=$!

for _ in $(seq 1 60); do
  curl -s -o /dev/null http://127.0.0.1:4071/api/health 2>/dev/null \
    && curl -s -o /dev/null http://127.0.0.1:4072/api/health 2>/dev/null && break
  sleep 0.5
done
TOK=$(mk_token)

# Provision the bench user/tenant (first authed call), then upgrade it to
# the `enterprise` plan so Node's token-bucket rate limiter (free plan trips
# 429 under N rapid samples and would make the comparison meaningless —
# PLAN_MULTIPLIER.enterprise === Infinity → middleware/rateLimit.js returns
# next() with no cap). C has no rate limiter, so both now do real work.
curl -s -o /dev/null -H "Authorization: Bearer $TOK" http://127.0.0.1:4071/api/me || true
curl -s -o /dev/null -H "Authorization: Bearer $TOK" http://127.0.0.1:4072/api/me || true
sqlite3 "$ROOT/server/data/japanmap.db" \
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
  read -r nmed nsize ncode <<<"$(samp http://127.0.0.1:4071 "$r")"
  read -r cmed csize ccode <<<"$(samp http://127.0.0.1:4072 "$r")"
  spd=$(awk -v n="$nmed" -v c="$cmed" 'BEGIN{ if(c>0 && n!="NA" && c!="NA") printf "%.2fx", n/c; else print "-" }')
  printf '%-34s %9ss %9ss %9s %12s %s/%s\n' "$r" "$nmed" "$cmed" "$spd" "${nsize}|${csize}" "$ncode" "$ccode"
done
echo
echo "N=$N samples/route (1 warmup dropped). Node :4071  C :4072  same japanmap.db."
echo "Sizes node|c; differing sizes => shape divergence to investigate (not just speed)."
