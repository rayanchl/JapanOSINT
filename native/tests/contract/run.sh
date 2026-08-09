#!/usr/bin/env bash
# P7 contract-parity harness — diff the C server against the committed Node
# fixtures.
#
# Real prod auth is Supabase RS256/JWKS (SUPABASE_JWT_SECRET is empty in .env),
# so forged tokens can't be accepted in prod mode. For ROUTE-LOGIC parity the
# fixtures were captured from the Node server under a "parity profile": HS256
# with a known test secret and JWKS disabled (SUPABASE_URL=""). The C server is
# started here under the identical profile. This isolates route/JSON parity
# from the prod auth mechanism (auth itself is covered by P3's 401/503
# byte-parity tests).
#
# THE FIXTURES ARE NOW IRREPLACEABLE — AND THIS SCRIPT USED TO DESTROY THEM.
# The Node backend (server/) was deleted on 2026-05-17 and is not in git
# history at a usable state, so `*.node.json` can never be re-captured on any
# host. This script nevertheless had a capture branch that spawned
# `node $ROOT/server/src/index.js`; it was guarded only by "is node installed",
# and node IS installed here, so a bare `./run.sh` took it. Node then failed
# instantly, but the capture loop ran
#     curl … > "$HERE/$f.node.json" || true
# and `>` truncates the file BEFORE curl runs, while `|| true` swallows the
# failure — one invocation zeroed ~8.7 MB of tracked baselines with a cheerful
# "captured … (0B)". The capture branch is gone. Every fetch in this file now
# writes to a temp file that is moved into place only after the fetch actually
# succeeded, so no failure path can shorten a fixture.
#
# Usage: run.sh                # start the C server, diff it against fixtures
set -euo pipefail
# Derive the repo root from this script's own location. It used to be the
# hardcoded absolute path of the machine the harness was written on
# (/Users/rayan/JapanOSINT), so the suite could not run anywhere else — on this
# WSL checkout it failed before executing a single test, which meant a whole
# audit ran with no regression suite available. `cd`-based resolution follows
# the checkout wherever it lives; JO_REPO_ROOT still overrides for odd layouts.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${JO_REPO_ROOT:-$(cd "$HERE/../../.." && pwd)}"
SECRET=parity-secret

if [ "${1:-}" = capture ]; then
  cat >&2 <<'EOF'
run.sh: capture mode is gone, permanently.
The fixtures were captured from server/src/index.js, which was deleted on
2026-05-17 and is not recoverable from git. The *.node.json files in this
directory ARE the baseline — treat them as read-only test data. If a route's
expected shape genuinely changes, edit the fixture deliberately and say why in
the commit message.
EOF
  exit 2
fi

# Neutralize ambient credential env for the C server. /api/status reports
# per-source `configured/set/missingVars` from the environment; whatever API
# keys happen to live in the developer's shell would otherwise make the run
# non-comparable against the key-less captured fixture. Same rationale as
# forcing SUPABASE_URL="" — the contract test isolates route logic, not the
# host's secret inventory. (List = the retired apiCredentials.js vars.)
CRED_UNSET="-u AERODATABOX_KEY -u EDINET_API_KEY -u ESTAT_API_KEY -u ESTAT_APP_ID -u FACEBOOK_ACCESS_TOKEN -u FOFA_API_KEY -u GITHUB_TOKEN -u GOOGLE_MYMAPS_IDS -u GRAYHAT_API_KEY -u GREYNOISE_API_KEY -u HOTPEPPER_API_KEY -u MARINETRAFFIC_API_KEY -u MISSKEY_TOKEN -u MLIT_N02_GEOJSON_URL -u MLS_API_KEY -u ODPT_CHALLENGE_TOKEN -u ODPT_CONSUMER_KEY -u ODPT_TOKEN -u OPENCELLID_KEY -u OPENCHARGEMAP_KEY -u OPENSKY_CLIENT_ID -u OPENSKY_CLIENT_SECRET -u QUAKE_API_KEY -u RESAS_API_KEY -u SENTINELHUB_CLIENT_ID -u SENTINELHUB_CLIENT_SECRET -u SHODAN_API_KEY -u TWITTER_BEARER_TOKEN -u UMISHIRU_API_KEY -u USGS_M2M_TOKEN -u VESSELFINDER_API_KEY -u WIGLE_API_KEY -u WINDY_API_KEY"
# KNOWN INTENTIONAL DIFF: /api/intel/sources no longer byte-matches Node and
# cannot. Node had ONE `camera-discovery` collector; the C port split it into
# 14+ registered sources that write under their own source_id, which left the
# parent aggregating to zero next to a dozen of its own channels. The C route
# now rolls those counts up and stamps each channel with `parent_id` (see
# intelapi.c). Node has no such key and no such rollup, so a DIFF on that one
# route is the expected result, not a regression. Every other route still gates.
ROUTES=(/api/status /api/sources /api/layers /api/intel/sources
        "/api/intel/items?limit=10")

have_node() { command -v node >/dev/null 2>&1; }

b64url() { openssl base64 -A | tr '+/' '-_' | tr -d '='; }

# HS256 JWT without Node, so the C side can be tested on a machine that has no
# Node install (which is now the normal case — nothing here needs Node).
mk_token_sh() {
  local h p sig
  h=$(printf '%s' '{"alg":"HS256","typ":"JWT"}' | b64url)
  p=$(printf '{"sub":"parity","aud":"authenticated","role":"authenticated","exp":%s}' \
        "$(( $(date +%s) + 86400 ))" | b64url)
  sig=$(printf '%s' "$h.$p" | openssl dgst -sha256 -hmac "$SECRET" -binary | b64url)
  printf '%s.%s.%s' "$h" "$p" "$sig"
}

mk_token() {
  if have_node; then
    node -e 'const c=require("crypto");const b=o=>Buffer.from(JSON.stringify(o)).toString("base64url");
    const h=b({alg:"HS256",typ:"JWT"}),p=b({sub:"parity",aud:"authenticated",role:"authenticated",exp:Math.floor(Date.now()/1e3)+86400});
    process.stdout.write(h+"."+p+"."+c.createHmac("sha256",process.argv[1]).update(h+"."+p).digest("base64url"))' "$SECRET"
  else
    mk_token_sh
  fi
}

# Free the C port before the run. A leftover *prod* japanosint on :4072 (real
# API keys in its env) would answer instead of ours and make /api/status show
# configured creds the parity profile must never see.
free_port() {
  local pids
  pids=$(lsof -ti tcp:4072 2>/dev/null || true)
  [ -n "$pids" ] && kill -9 $pids 2>/dev/null || true
  sleep 1
}

start_c() {
  free_port
  SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" \
    env $CRED_UNSET PORT=4072 "$ROOT/native/bin/japanosint" --serve \
    >/tmp/jo_parity_c.log 2>&1 &
  echo $!
  for _ in $(seq 1 30); do curl -s http://127.0.0.1:4072/api/health >/dev/null 2>&1 && return; sleep 0.3; done
}

# Fetch a route into $2, atomically. curl writes to a temp file and the result
# is moved into place ONLY if curl exited 0 and produced bytes; on any failure
# the previous file is left exactly as it was. This is the whole reason the
# fixture-destroying bug above cannot recur.
fetch_to() {
  local url="$1" dest="$2" tmp
  tmp="$(mktemp "${TMPDIR:-/tmp}/jo_contract.XXXXXX")"
  if curl -s --max-time 60 -H "Authorization: Bearer $TOK" "$url" >"$tmp" \
     && [ -s "$tmp" ]; then
    mv "$tmp" "$dest"
    return 0
  fi
  rm -f "$tmp"
  return 1
}

[ -x "$ROOT/native/bin/japanosint" ] \
  || { echo "no $ROOT/native/bin/japanosint — run: make -C native -j" >&2; exit 2; }

TOK=$(mk_token)

# The fixtures are the contract. Refuse to run against a missing or emptied
# one rather than silently reporting 0/5.
#
# Which files here are what, since three suffixes is two too many:
#   *.node.json  the baseline. Read-only, irreplaceable, compared against.
#   *.c.json     this run's C output. Overwritten every run; not an input.
#   *.json       superseded duplicates from the original Node-import commit
#                (c7e4390). NOTHING reads them — grep the tree. Kept only
#                because they are the pre-`.node.json` capture of the same
#                routes; do not "fix" them and do not compare against them.
missing=0
for r in "${ROUTES[@]}"; do
  f=$(echo "$r" | sed 's/[/?=&]/_/g')
  [ -s "$HERE/$f.node.json" ] || { echo "MISSING/EMPTY fixture $f.node.json"; missing=1; }
done
if [ "$missing" = 1 ]; then
  echo "fixtures are unrecoverable (server/ is gone) — restore them with:" >&2
  echo "  git checkout -- native/tests/contract/" >&2
  exit 2
fi

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

# Itemize a source-set DIFF instead of just reporting one.
#
# The *.node.json baselines name sources that no longer exist — boj-stats,
# jcg-navarea, nict-atlas, twitch-jp-streams, xrain-radar and 60 others whose
# collectors were deleted in the 66-source removal sweep. Those references are
# CORRECT: the fixture is a capture of what Node served in May 2026, it can
# never be re-captured (server/ is gone), and rewriting it to match today would
# destroy the only record of the contract. So the ids stay, and instead the
# harness says out loud which ids moved, so a reader can tell an intentional
# retirement from a regression without diffing 6 MB by hand.
id_delta() {
  command -v python3 >/dev/null 2>&1 || return 0
  python3 - "$1" "$2" <<'PY'
import json, sys


def ids(path):
    try:
        d = json.load(open(path, encoding="utf-8"))
    except Exception:
        return None
    rows = d.get("apis") or d.get("data") if isinstance(d, dict) else d
    if not isinstance(rows, list):
        return None
    return {r["id"] for r in rows if isinstance(r, dict) and isinstance(r.get("id"), str)}


a, b = ids(sys.argv[1]), ids(sys.argv[2])
if a is None or b is None:
    sys.exit(0)
gone, new = sorted(a - b), sorted(b - a)
print("          ids: node %d, c %d — retired %d, added %d"
      % (len(a), len(b), len(gone), len(new)))
if gone:
    print("          retired: %s%s"
          % (", ".join(gone[:8]), " …" if len(gone) > 8 else ""))
PY
}

CPID=$(start_c)
trap 'kill "$CPID" 2>/dev/null || true' EXIT
pass=0; tot=0
for r in "${ROUTES[@]}"; do
  f=$(echo "$r" | sed 's/[/?=&]/_/g'); tot=$((tot+1))
  if ! fetch_to "http://127.0.0.1:4072$r" "$HERE/$f.c.json"; then
    echo "FETCH   $r — C server returned nothing (see /tmp/jo_parity_c.log)"
    continue
  fi
  if diff -q <(norm "$HERE/$f.node.json") <(norm "$HERE/$f.c.json") >/dev/null 2>&1; then
    echo "PARITY  $r"; pass=$((pass+1))
  else
    echo "DIFF    $r (node $(wc -c <"$HERE/$f.node.json")B vs c $(wc -c <"$HERE/$f.c.json")B)"
    id_delta "$HERE/$f.node.json" "$HERE/$f.c.json"
  fi
done
echo "CONTRACT PARITY: $pass/$tot"
