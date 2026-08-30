#!/usr/bin/env bash
# tests/contract/rank_contract.sh — contract of /api/intel/items ranking,
# totals, snippets and collapse (intelapi.c). Unlike run.sh this is not a
# byte-parity diff against a Node capture (Node never had these parameters);
# it asserts the SHAPE the C server promises, on whatever database it is
# pointed at:
#
#   * sort=relevance|trust without q, or an unknown sort, is a 400 whose body
#     is exactly rank_errors.fixture (in-band reason, never a silent date feed)
#   * sort=relevance returns `rank` per item, non-increasing down the page,
#     and a cursor that resumes strictly after the last row (no overlap)
#   * sort=trust returns meta.rerank {window, candidates, of|of_gte, bounded}
#     and a per-item `rerank` breakdown — the bounded view says it is bounded
#   * total=1 fills page.total (or page.total_gte, never both)
#   * q= adds `snippet` to every item; collapse=1 adds meta.collapsed and
#     never emits two rows with the same cluster_id
#   * a cursor of the wrong kind for the sort is disclosed as
#     meta.notes["cursor_ignored"]
#
# Usage:  JO_DB=/path/to/warm.db tests/contract/rank_contract.sh [query]
# With an empty DB the 400 and envelope checks still run; the ordering checks
# need at least one page of hits for the query (default: "security").
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${JO_REPO_ROOT:-$(cd "$HERE/../../.." && pwd)}"
BIN="$ROOT/native/bin/japanosint"
Q="${1:-security}"
PORT="${PORT:-4074}"
SECRET=rank-contract-secret
[ -x "$BIN" ] || { echo "no $BIN — run: make -C native -j" >&2; exit 2; }
command -v python3 >/dev/null || { echo "python3 required" >&2; exit 2; }

export JO_DB="${JO_DB:-$(mktemp -d)/rank_contract.db}"
[ -s "$JO_DB" ] || "$BIN" --selftest >/dev/null 2>&1 || true

b64url() { openssl base64 -A | tr '+/' '-_' | tr -d '='; }
h=$(printf '%s' '{"alg":"HS256","typ":"JWT"}' | b64url)
p=$(printf '{"sub":"rank","aud":"authenticated","role":"authenticated","exp":%s}' "$(( $(date +%s) + 3600 ))" | b64url)
sig=$(printf '%s' "$h.$p" | openssl dgst -sha256 -hmac "$SECRET" -binary | b64url)
TOK="$h.$p.$sig"

SUPABASE_URL="" SUPABASE_JWT_SECRET="$SECRET" PORT=$PORT "$BIN" --serve >/tmp/jo_rank_contract.log 2>&1 &
CPID=$!
trap 'kill "$CPID" 2>/dev/null || true' EXIT
for _ in $(seq 1 40); do curl -s "http://127.0.0.1:$PORT/api/health" >/dev/null 2>&1 && break; sleep 0.3; done

OUT="$(mktemp -d)"
get() {  # get <name> <path>  → $OUT/<name>.json, prints status
  curl -s -o "$OUT/$1.json" -w "%{http_code}" -H "Authorization: Bearer $TOK" "http://127.0.0.1:$PORT$2"
}

fail=0
chk() { if [ "$1" = "$2" ]; then echo "  ok    $3"; else echo "  FAIL  $3 (got '$1', want '$2')"; fail=1; fi; }

echo "rank contract against $JO_DB (q=$Q)"
chk "$(get e1 "/api/intel/items?sort=relevance")" 400 "sort=relevance without q is 400"
chk "$(get e2 "/api/intel/items?sort=trust")"     400 "sort=trust without q is 400"
chk "$(get e3 "/api/intel/items?sort=bogus&q=x")" 400 "unknown sort is 400"
chk "$(get e4 "/api/intel/items?sort=relevance&q=%27%27%27")" 400 "q with no searchable token cannot be ranked"
cat "$OUT/e1.json" "$OUT/e2.json" "$OUT/e3.json" "$OUT/e4.json" | sed 's/}{/}\n{/g' > "$OUT/errors.txt"; echo >> "$OUT/errors.txt"
if diff -q "$HERE/rank_errors.fixture" "$OUT/errors.txt" >/dev/null; then echo "  ok    400 bodies match rank_errors.fixture"
else echo "  FAIL  400 bodies differ from rank_errors.fixture:"; diff "$HERE/rank_errors.fixture" "$OUT/errors.txt" || true; fail=1; fi

chk "$(get d1 "/api/intel/items?limit=5&total=1&collapse=1")" 200 "date feed with total+collapse"
chk "$(get r1 "/api/intel/items?limit=5&q=$Q&sort=relevance&total=1&collapse=1")" 200 "relevance page 1"
chk "$(get t1 "/api/intel/items?limit=5&q=$Q&sort=trust")" 200 "trust page 1"
chk "$(get w1 "/api/intel/items?limit=5&q=$Q&sort=relevance&cursor=$(printf '{"p":"x","u":"y"}' | b64url)")" 200 "wrong-kind cursor"

python3 - "$OUT" "$TOK" "$PORT" "$Q" <<'PY' || fail=1
import json, sys, urllib.request, base64
out, tok, port, q = sys.argv[1:5]
bad = 0
def ok(cond, msg):
    global bad
    print(("  ok    " if cond else "  FAIL  ") + msg)
    if not cond: bad += 1
def load(n): return json.load(open(f"{out}/{n}.json"))
def fetch(path):
    r = urllib.request.Request(f"http://127.0.0.1:{port}{path}", headers={"Authorization": f"Bearer {tok}"})
    return json.load(urllib.request.urlopen(r))

d1 = load("d1")
ok(("total" in d1["page"]) and (d1["page"]["total"] is not None) != ("total_gte" in d1["page"]),
   "total=1 → page.total XOR page.total_gte")
ok(d1["meta"].get("collapsed") == 0 or isinstance(d1["meta"].get("collapsed"), int), "collapse=1 → meta.collapsed")
ok("sort" not in d1["meta"], "date feed carries no meta.sort")

r1 = load("r1")
ok(r1["meta"].get("sort") == "relevance", "meta.sort=relevance")
ok(r1["meta"].get("q_applied") is True, "meta.q_applied")
ranks = [it.get("rank") for it in r1["data"]]
ok(all(isinstance(x, (int, float)) for x in ranks), f"every item has a numeric rank ({len(ranks)} rows)")
ok(ranks == sorted(ranks, reverse=True), "rank is non-increasing down the page")
ok(all("snippet" in it for it in r1["data"]), "q= → every item has a snippet key")
cl = [it["cluster_id"] for it in r1["data"] if it.get("cluster_id")]
ok(len(cl) == len(set(cl)), "collapse=1 → no two rows share a cluster_id")
cur = r1["page"]["next_cursor"]
if cur:
    r2 = fetch(f"/api/intel/items?limit=5&q={q}&sort=relevance&cursor={cur}")
    u1 = {it["uid"] for it in r1["data"]}; u2 = {it["uid"] for it in r2["data"]}
    ok(not (u1 & u2), "relevance cursor resumes without overlap")
    ok(all(x <= min(ranks) for x in [it["rank"] for it in r2["data"]]), "page 2 ranks ≤ page 1 minimum")
    tok_ = json.loads(base64.urlsafe_b64decode(cur + "=" * (-len(cur) % 4)))
    ok(set(tok_) == {"r", "u"}, "relevance cursor is {r,u}")
else:
    print("  skip  relevance cursor (fewer than 6 hits in this DB)")

t1 = load("t1")
rr = t1["meta"].get("rerank", {})
ok(t1["meta"].get("sort") == "trust", "meta.sort=trust")
ok(isinstance(rr.get("window"), int) and "candidates" in rr and ("of" in rr or "of_gte" in rr) and "bounded" in rr,
   "meta.rerank states window/candidates/of|of_gte/bounded")
ok(all(set(it.get("rerank", {})) >= {"bm25", "trust", "decay", "score"} for it in t1["data"]),
   "every trust row carries its rerank breakdown")
sc = [it["rerank"]["score"] for it in t1["data"]]
ok(sc == sorted(sc, reverse=True), "trust score is non-increasing down the page")

w1 = load("w1")
ok("cursor_ignored" in w1["meta"].get("notes", []), "wrong-kind cursor disclosed as meta.notes cursor_ignored")
sys.exit(1 if bad else 0)
PY

if [ "$fail" = 0 ]; then echo "RANK CONTRACT: OK"; else echo "RANK CONTRACT: FAIL"; exit 1; fi
