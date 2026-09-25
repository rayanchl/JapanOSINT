#!/usr/bin/env bash
# ============================================================================
# JapanOSINT — native backend launch orchestrator
# ----------------------------------------------------------------------------
# Pods (independently toggleable):
#   build    cd native && make  (INCREMENTAL — only rebuilds changed objects)
#   server   ./bin/japanosint --serve   (httpd + background scheduler+enricher)
#   web      client/ Vite dev server — the site (:5173); /api → the server pod
#   llama    native/llama/llama-server  (Qwen3.8-27B, :8080)
#   maint    one-shot `--run llm-enricher` to warm the entity graph (optional;
#            ongoing enrichment already runs inside the server scheduler)
#
# Usage:
#   ./launch.sh up              freeze → build(if needed) → server → llama → status
#   ./launch.sh up --maint      …also kick the maintenance/enricher pod once
#   ./launch.sh build [--rebuild]
#   ./launch.sh serve | web | web-stop | llama | maint | status | down | logs [server|web|llama]
#   ./launch.sh tags            print EVERY launch tag/env knob (grouped)
#
# Pod skips / knobs (any subset):  --no-build --no-server --no-web --no-llama --maint
#   --rebuild  --force  --port N  --web-port N  --db PATH  --env-file PATH  --model PATH
#
# "Stop the other session": kills stale japanosint/llama-server WE started.
# It will NOT kill a concurrent `make` (another editor/agent build) — it
# detects one and refuses with a message unless you pass --force. A script
# cannot stop a Claude *session*; close that yourself before a clean run.
# ============================================================================
set -euo pipefail

# ---- paths -----------------------------------------------------------------
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NATIVE="$REPO/native"
BIN="$NATIVE/bin/japanosint"
# pidfiles + logs (gitignore native/.run/). Overridable because the default
# lives on /mnt/c, where a Windows process holding a log open makes the file
# unwritable from WSL — the advice "set RUN=<dir>" is only true if this honours
# the environment, and it did not.
: "${RUN:=$NATIVE/.run}"
mkdir -p "$RUN"

# ---- launch tags (env wins → flags override → these prod defaults) ---------
# Runtime / server pod
: "${PORT:=4000}"                                   # httpd port
# Remember whether the CALLER chose the database, before the default lands —
# `: "${JO_DB:=...}"` makes "unset" and "explicitly set" indistinguishable a
# line later, and .env must not override a deliberate `JO_DB=... ./launch.sh`
# or `--db`. Resolved against .env further down, once envget() exists.
JO_DB_FROM_CALLER="${JO_DB:-}"
: "${JO_DB:=$REPO/data/japanmap.db}"                # sqlite db
: "${JO_SCHEMA:=$NATIVE/core/schema.sql}"           # schema (auto-applied)
: "${JO_ENV_FILE:=$REPO/.env}"                      # binary self-loads this
: "${JO_NO_SCHED:=}"                                # set=1 → serve w/o scheduler
# Auth pod (prod)
: "${SUPABASE_URL:=}"                               # JWKS/RS256 issuer (PROD)
: "${SUPABASE_JWT_SECRET:=}"                        # HS256 fallback
: "${SUPABASE_AUD:=}"                               # expected aud claim (else .env/binary default)
# LLM / llama pod
: "${LLM_ENABLED:=true}"                            # gate enricher+pipeline
: "${LLM_BASE_URL:=http://127.0.0.1:8080}"          # llama-server endpoint
: "${LLM_MODEL:=}"                                  # optional model-name pin
: "${LLAMA_BIN:=$NATIVE/llama/llama-server}"
: "${LLAMA_MODEL:=$REPO/models/Qwen3.8-27B-Q8_0.gguf}"
: "${LLAMA_PORT:=8080}"
: "${LLAMA_CTX:=16384}"
: "${LLAMA_HOST:=127.0.0.1}"
# Suggest LLM pod — dedicated small/fast model for /api/search/suggest so it
# never contends with the pipeline's 20B (falls back to LLM_BASE_URL in-binary)
: "${LLM_SUGGEST_BASE_URL:=http://127.0.0.1:8081}"  # binary reads this for suggest
: "${LLAMA_SUGGEST_MODEL:=$REPO/models/qwen2.5-1.5b-instruct-q4_k_m.gguf}"
: "${LLAMA_SUGGEST_PORT:=8081}"
: "${LLAMA_SUGGEST_CTX:=4096}"
# optional embedding pod (hybrid semantic search, core/embed_pod.c). The C
# binary's pod is inert unless JO_EMBED_URL is set, and it is set below ONLY
# when this pod actually starts — so a missing model means an honest
# "disabled" in /api/intel/semantic's coverage block, never a pod retrying a
# port nothing listens on. Model is NOT downloaded: drop bge-m3 (1024-d) or
# multilingual-e5-small/-base (384/768-d) into models/ or point at it.
: "${LLAMA_EMBED_MODEL:=$REPO/models/bge-m3-Q8_0.gguf}"
: "${LLAMA_EMBED_PORT:=8082}"
# Must hold a WHOLE embed batch (JO_EMBED_BATCH inputs), not one input — see
# cmd_llama_embed. bge-m3's native context is 8192.
: "${LLAMA_EMBED_CTX:=8192}"
: "${LLAMA_EMBED_POOLING:=}"                        # cls|mean|last; empty = GGUF metadata
: "${JO_EMBED_URL:=}"                               # set by cmd_llama_embed when the pod is up
# Web pod — client/ Vite dev server; its /api proxy targets the server pod
: "${WEB_PORT:=5173}"
: "${WEB_HOST:=127.0.0.1}"                          # 0.0.0.0 to expose the site on the LAN
: "${WEB_RUNTIME:=}"                                # windows|native; empty = auto (see cmd_web)
: "${WEB_WAIT:=60}"                                 # s to wait for vite to answer
# Orchestrator behaviour
# Expected `--list-sources` count. Empty => derive it from the source tree via
# native/tools/lint_sources.py --count. It used to be hardcoded to 476, which
# was one of SIX different wrong source counts committed across this repo; any
# number typed in here is stale the next time a collector lands. Set it
# explicitly only to pin an expectation for a release check.
: "${EXPECT_SOURCES:=}"                             # warn if registry differs
: "${LLAMA_WAIT:=600}"                              # s to wait for model load (27G Qwen over /mnt/c)
: "${SERVER_WAIT:=30}"                              # s to wait for httpd listen

DO_BUILD=1 DO_SERVER=1 DO_WEB=1 DO_LLAMA=1 DO_SUGGEST_LLAMA=1 DO_EMBED_LLAMA=1 DO_MAINT=0 REBUILD=0 FORCE=0

# ---- ui helpers ------------------------------------------------------------
c(){ printf '\033[%sm' "$1"; }; NC=$(c 0); B=$(c '1;36'); G=$(c '1;32'); Y=$(c '1;33'); R=$(c '1;31')
say(){ printf '%s[launch]%s %s\n' "$B" "$NC" "$*"; }
ok(){  printf '%s  ✓%s %s\n' "$G" "$NC" "$*"; }
warn(){ printf '%s  ! %s%s\n' "$Y" "$*" "$NC"; }
die(){ printf '%s  ✗ %s%s\n' "$R" "$*" "$NC" >&2; exit 1; }

# ---- arg parse -------------------------------------------------------------
CMD="${1:-up}"; [ $# -gt 0 ] && shift || true
while [ $# -gt 0 ]; do case "$1" in
  --no-build)  DO_BUILD=0 ;;
  --no-server) DO_SERVER=0 ;;
  --no-llama)  DO_LLAMA=0 ;;
  --no-suggest-llama) DO_SUGGEST_LLAMA=0 ;;
  --no-embed-llama) DO_EMBED_LLAMA=0 ;;
  --no-web)    DO_WEB=0 ;;
  --web-port)  WEB_PORT="$2"; shift ;;
  --maint)     DO_MAINT=1 ;;
  --rebuild)   REBUILD=1 ;;
  --force)     FORCE=1 ;;
  --port)      PORT="$2"; shift ;;
  # Mark it as caller-chosen as well: the .env resolution below only defers to
  # the caller, and arg parsing runs AFTER the JO_DB_FROM_CALLER snapshot, so
  # without this a `--db` would be silently overridden by the .env pin.
  --db)        JO_DB="$2"; JO_DB_FROM_CALLER="$2"; shift ;;
  --env-file)  JO_ENV_FILE="$2"; shift ;;
  --model)     LLAMA_MODEL="$2"; shift ;;
  -h|--help)   CMD="tags" ;;
  *) die "unknown flag: $1 (try: ./launch.sh tags)" ;;
esac; shift; done

pidfile(){ echo "$RUN/$1.pid"; }
logfile(){ echo "$RUN/$1.log"; }
alive(){ local f; f="$(pidfile "$1")"; [ -f "$f" ] && kill -0 "$(cat "$f")" 2>/dev/null; }

# Effective value of an env var the binary reads: real env wins, else the
# value in $JO_ENV_FILE (which the binary self-loads via setenv(...,0)), else
# "". Mirrors the binary's own precedence so status/tags tell the truth.
envget(){ local k="$1" v="${!1:-}"
  [ -n "$v" ] && { printf '%s' "$v"; return; }
  # `tr -d '\r'`: .env is edited on Windows and is CRLF, so every value this
  # returned carried a trailing carriage return. Harmless in a `[ -n ... ]`
  # presence check, which is all this was used for at first — and wrong the
  # moment a value is COMPARED or used as a path, where "…/japanmap.db\r" is
  # simply a file that does not exist.
  [ -f "$JO_ENV_FILE" ] && sed -nE "s/^[[:space:]]*$k=[\"']?([^\"'#]*)[\"']?.*/\1/p" "$JO_ENV_FILE" | head -1 | tr -d '\r'; }

# JO_DB PINNED IN .env WINS OVER THE BUILT-IN DEFAULT.
#
# The database was moved onto WSL's native ext4 (2026-09-15): on /mnt/c it is
# reached over 9p, where a `PRAGMA quick_check` took 413 s against 47.7 s on
# ext4 — and a full disk on that volume is what corrupted the FTS index in the
# first place. The new path is recorded in .env so every entry point agrees,
# but launch.sh had a hardcoded default that silently won, which would have
# split reads and writes across two different database files.
#
# Precedence, highest first: `--db` / an exported JO_DB (the caller meant it),
# then .env, then the in-repo default.
if [ -z "$JO_DB_FROM_CALLER" ]; then
  # NOT envget: it starts with `v="${!1:-}"`, an indirect expansion of the
  # variable named by its argument — and JO_DB already holds the default by
  # now, so envget would hand that default straight back and never look in
  # .env. Read the file directly for this one.
  _env_db=""
  [ -f "$JO_ENV_FILE" ] && _env_db="$(sed -nE "s/^[[:space:]]*JO_DB=[\"']?([^\"'#]*)[\"']?.*/\1/p" "$JO_ENV_FILE" | head -1 | tr -d '\r')"
  [ -n "$_env_db" ] && JO_DB="$_env_db"
  unset _env_db
fi
[ -f "$JO_DB" ] || [ -n "$JO_DB_FROM_CALLER" ] || {
  # A pinned path that does not exist is a typo or an unmounted disk, and
  # SQLite would silently CREATE an empty database there — an empty app that
  # looks healthy. Say so instead.
  case "$CMD" in serve|up) warn "JO_DB=$JO_DB does not exist; a new empty database would be created there"; esac
}
# Build the env prefix for the binary: orchestrator-owned vars always passed
# (we chose port/db/llama target); secrets passed ONLY if explicitly in the
# real env — an empty pass-through would shadow the .env value the binary
# loads itself (main.c setenv(...,0) won't overwrite an already-set var).
binenv(){ BINENV=( "PORT=$PORT" "JO_DB=$JO_DB" "JO_SCHEMA=$JO_SCHEMA"
    "JO_ENV_FILE=$JO_ENV_FILE" "LLM_ENABLED=$LLM_ENABLED" "LLM_BASE_URL=$LLM_BASE_URL"
    "LLM_SUGGEST_BASE_URL=$LLM_SUGGEST_BASE_URL" )
  [ -n "${JO_NO_SCHED:-}" ]        && BINENV+=( "JO_NO_SCHED=$JO_NO_SCHED" )
  [ -n "${SUPABASE_URL:-}" ]       && BINENV+=( "SUPABASE_URL=$SUPABASE_URL" )
  [ -n "${SUPABASE_JWT_SECRET:-}" ]&& BINENV+=( "SUPABASE_JWT_SECRET=$SUPABASE_JWT_SECRET" )
  [ -n "${SUPABASE_AUD:-}" ]       && BINENV+=( "SUPABASE_AUD=$SUPABASE_AUD" )
  [ -n "${LLM_MODEL:-}" ]          && BINENV+=( "LLM_MODEL=$LLM_MODEL" );
  [ -n "${JO_EMBED_URL:-}" ]       && BINENV+=( "JO_EMBED_URL=$JO_EMBED_URL" ); }

# wait until $1 (a log file) contains regex $2, up to $3 seconds
wait_log(){ local f="$1" re="$2" t="$3" e=$((SECONDS+$3))
  while [ $SECONDS -lt $e ]; do grep -qE "$re" "$f" 2>/dev/null && return 0; sleep 1; done
  return 1; }
# poll an http url for 200/expected until ok or $2 seconds
wait_http(){ local url="$1" t="$2" e=$((SECONDS+$2)) code
  while [ $SECONDS -lt $e ]; do
    code=$(curl -fsS -m3 -o /dev/null -w '%{http_code}' "$url" 2>/dev/null || true)
    [ "$code" = "200" ] && return 0; sleep 2
  done; return 1; }

# ---- tags reference (auto-derived so it can't go stale) --------------------
cmd_tags(){
  local su sj sa
  su="$(envget SUPABASE_URL)"; sj="$(envget SUPABASE_JWT_SECRET)"; sa="$(envget SUPABASE_AUD)"
cat <<EOF
${B}JapanOSINT launch tags${NC}  (precedence: real env  →  CLI flag  →  default
                       →  $JO_ENV_FILE which the binary self-loads, env wins)

${G}POD SELECTION (./launch.sh up ...)${NC}
  --no-build         skip make (use existing bin/japanosint)
  --no-server        don't start the http+scheduler pod
  --no-llama         don't start llama-server (server still boots; LLM degrades)
  --no-suggest-llama don't start the suggest llama pod (suggest falls back to :8080)
  --no-embed-llama   don't start the embedding llama pod (/api/intel/semantic → 503)
  --no-web           don't start the web pod (the site)
  --maint            after server is up, run llm-enricher once (warm graph)
  --rebuild          make clean && make (full)        --force  skip make-guard
  --port N  --web-port N  --db PATH  --env-file PATH  --model PATH

${G}SERVER / RUNTIME POD  (read by bin/japanosint)${NC}
  PORT=$PORT                 httpd port            (binary default 4000)
  JO_DB=$JO_DB
  JO_SCHEMA=...schema.sql      auto-applied on open
  JO_ENV_FILE=$JO_ENV_FILE   binary loads it itself
  JO_NO_SCHED=${JO_NO_SCHED:-<unset>}            set→serve WITHOUT scheduler/enricher
  CLI: --serve  --run <id> [entity]  --list-sources  --sched  --wakati  (none=selftest)

${G}WEB POD  (client/ Vite dev server — the site)${NC}
  WEB_PORT=$WEB_PORT  WEB_HOST=$WEB_HOST  → http://$WEB_HOST:$WEB_PORT/
  WEB_RUNTIME=${WEB_RUNTIME:-<auto: $(web_is_windows && echo 'windows node.exe' || echo 'native node')>}
  /api proxied to http://127.0.0.1:$PORT (JO_API_TARGET, read by client/vite.config.js)
  ./launch.sh web | web-stop | logs web

${G}AUTH POD (prod JWT)${NC}
  SUPABASE_URL=${su:-<unset — REQUIRED for prod RS256/JWKS>}
  SUPABASE_JWT_SECRET=$([ -n "$sj" ] && echo '<set>' || echo '<unset>')   HS256 fallback
  SUPABASE_AUD=${sa:-<unset → binary/.env default>}
  (effective values above: real env wins, else read from $JO_ENV_FILE)
  Break-glass/admin (optional): BREAK_GLASS_ENABLED BREAK_GLASS_JWT_SECRET
  ADMIN_TOTP_SECRET SECRETS_MASTER_KEY PLATFORM_OPERATOR_EMAILS PLATFORM_OPERATOR_IDS

${G}LLM / LLAMA POD${NC}
  LLM_ENABLED=$LLM_ENABLED              gate enricher+pipeline (Node parity)
  LLM_BASE_URL=$LLM_BASE_URL
  LLM_MODEL=${LLM_MODEL:-<unset>}   LLM_HEAVY_CONCURRENCY  LLM_MID_CONCURRENCY
  LLAMA_BIN=$LLAMA_BIN
  LLAMA_MODEL=$LLAMA_MODEL
  LLAMA_PORT=$LLAMA_PORT  LLAMA_CTX=$LLAMA_CTX  LLAMA_HOST=$LLAMA_HOST
  launch args: -m <model> --port $LLAMA_PORT --host $LLAMA_HOST \\
               --ctx-size $LLAMA_CTX --jinja --reasoning-format auto -fa auto

${G}MAINTENANCE / ENRICH POD${NC}
  In-process: the server scheduler runs source 'llm-enricher' every 300s
              (gated by LLM_ENABLED) — NER → entity graph, tier-2 dedup.
  One-shot  : ./launch.sh maint   (= bin/japanosint --run llm-enricher)
  Also runnable: ./launch.sh -- any --run <source_id> via bin/japanosint

${G}OSINT SOURCE API KEYS${NC}  (optional; absent key → that source no-ops, faithful)
$(grep -rhoE 'getenv\("[A-Z0-9_]+"\)' "$NATIVE"/collectors/*/sources/*.c "$NATIVE"/lib/*.c 2>/dev/null \
   | sed -E 's/getenv\("([A-Z0-9_]+)"\)/\1/' | sort -u | grep -E '_(API_)?(KEY|TOKEN|SECRET|ID|EMAIL|USER|APP_ID)$' \
   | grep -vE '^(JO_|SUPABASE_|LLM_|BREAK_GLASS_|ADMIN_|SECRETS_|PLATFORM_)' \
   | awk '{printf "  %-26s", $0; if (NR%3==0) print ""} END{print ""}')
EOF
}

# ---- freeze: stop OUR stale procs; guard a concurrent make ----------------
cmd_freeze(){
  say "freeze: stopping anything we previously launched"
  for p in server llama llama-suggest llama-embed; do
    if alive "$p"; then kill "$(cat "$(pidfile "$p")")" 2>/dev/null && ok "stopped $p pod"; fi
    rm -f "$(pidfile "$p")"
  done
  # Stragglers not tracked by a pidfile. Match the BASENAME, not "$BIN": a
  # server started from native/ as `./bin/japanosint --serve` carries a
  # RELATIVE path in its cmdline, which a pattern built from the absolute $BIN
  # never matches. Such a stray then keeps $PORT and the next launch dies 30s
  # later with a bare "httpd did not report listening" — while `tail -20` shows
  # the STRAY's log spam (it appends to the same file) instead of the new
  # server's own "[httpd] cannot bind" line. One orphan cost three days of a
  # wedged server writing to the production DB before this was noticed.
  if pkill -f "$(basename "$BIN") --serve" 2>/dev/null; then
    ok "killed stray japanosint --serve"
    # pkill returns once the signal is SENT, not once the process is gone;
    # wait for it to actually release the port before we try to bind it.
    for _ in $(seq 1 40); do
      pgrep -f "$(basename "$BIN") --serve" >/dev/null 2>&1 || break
      sleep 0.25
    done
  fi
  pkill -f "llama-server.*Qwen3.8-27B"   2>/dev/null && ok "killed stray llama-server"        || true
  pkill -f "llama-server.*qwen2.5-1.5b"  2>/dev/null && ok "killed stray suggest llama-server" || true
  web_stop
  # concurrent build guard (don't trample another editor/agent compiling native/)
  if pgrep -fl "make .*-j|cc1 .*native|clang .*native/" >/dev/null 2>&1; then
    if [ "$FORCE" = 1 ]; then warn "concurrent make detected — proceeding (--force)"
    else die "a 'make' is compiling native/ (concurrent session). Close it or pass --force."; fi
  fi
  ok "freeze complete"
}

# ---- build: incremental; verify registry ----------------------------------
cmd_build(){
  command -v make >/dev/null || die "make not found"
  if [ "$REBUILD" = 1 ]; then say "build: make clean && make (--rebuild)"; make -C "$NATIVE" clean >/dev/null
  elif make -C "$NATIVE" -q >/dev/null 2>&1 && [ -x "$BIN" ]; then
    ok "build: up-to-date, skipping (bin/japanosint current)"; cmd_build_verify; return
  else say "build: incremental make (sources changed)"; fi
  # make's exit status is the ONLY honest signal here. This used to put make
  # first in a pipeline AND wrap the whole thing in `if ! …; then :; fi`, which
  # threw the status away twice (the second form defeats `pipefail` too),
  # leaving `[ -x "$BIN" ]` as the only check — and a STALE binary is still
  # executable, so a broken compile booted the previous build while printing
  # "build green". tests/audit/report_01.md:132-135 records an entire audit run
  # against a 40-minute-stale binary because of exactly this. Same fix as
  # tests/audit/agent_build.sh:14-18, carried back at last.
  local log; log="$(logfile build)"
  if ! make -C "$NATIVE" -j8 >"$log" 2>&1; then
    grep -iE 'error:|undefined reference|fatal' "$log" \
      | grep -viE 'deprecated|SHA1|MD5|RSA_|EC_KEY|ECDSA_|EVP_PKEY_assign' \
      | tail -25 || true
    die "build FAILED — $BIN was NOT updated; do not trust a run against it (full log: $log)"
  fi
  grep -E 'warning: .*(uninitial|overflow|format)' "$log" | head -10 || true
  [ -x "$BIN" ] || die "build failed — make succeeded but $BIN is missing (see $log)"
  cmd_build_verify
}
cmd_build_verify(){
  # Print the binary's mtime so staleness is visible at a glance rather than
  # inferred from a green line.
  local n exp mt
  n=$("$BIN" --list-sources 2>/dev/null | wc -l | tr -d ' ')
  # GNU and BSD stat disagree; `date -r` means "epoch seconds" on macOS, so it
  # is not a portable mtime reader. Try both stats, then give up honestly.
  mt=$(stat -c '%y' "$BIN" 2>/dev/null | cut -d. -f1) \
    || mt=$(stat -f '%Sm' -t '%Y-%m-%d %H:%M:%S' "$BIN" 2>/dev/null) || mt='?'
  [ -n "$mt" ] || mt='?'
  exp="$EXPECT_SOURCES"
  # Derive the expectation from the source tree instead of a hardcoded number.
  if [ -z "$exp" ] && command -v python3 >/dev/null 2>&1 \
     && [ -f "$NATIVE/tools/lint_sources.py" ]; then
    exp=$(python3 "$NATIVE/tools/lint_sources.py" --count 2>/dev/null \
          | sed -nE 's/^registered source_defs: ([0-9]+).*/\1/p')
  fi
  if [ -z "$exp" ] || [ "$n" = "$exp" ]; then
    ok "build green — $n sources registered (binary built $mt)"
  else
    warn "build green but $n sources registered; the source tree defines $exp (binary built $mt)"
  fi
}

# ---- server pod ------------------------------------------------------------
cmd_serve(){
  [ -x "$BIN" ] || die "no bin/japanosint — run: ./launch.sh build"
  alive server && { warn "server pod already running (pid $(cat "$(pidfile server)"))"; return; }
  if [ -n "$(envget SUPABASE_URL)" ]; then ok "auth: SUPABASE_URL present (via ${SUPABASE_URL:+env}${SUPABASE_URL:-$JO_ENV_FILE})"
  else warn "SUPABASE_URL not in env or $JO_ENV_FILE — prod RS256/JWKS off (HS256/dev only)"; fi
  # A port still held after freeze means a stray nothing above could identify.
  # Say so NOW, naming the holder, instead of waiting 30s to report a symptom.
  if command -v ss >/dev/null 2>&1 && ss -ltn 2>/dev/null | grep -q ":$PORT "; then
    ss -ltnp 2>/dev/null | grep ":$PORT " >&2 || true
    die "port $PORT is already in use — stop that process first (it is not one of our pidfiles)"
  fi
  local lg; lg="$(logfile server)"; : >"$lg"
  say "server: bin/japanosint --serve  (PORT=$PORT DB=$JO_DB LLM_ENABLED=$LLM_ENABLED)"
  binenv
  nohup env "${BINENV[@]}" "$BIN" --serve >>"$lg" 2>&1 &
  echo $! >"$(pidfile server)"
  # Prefer the server's OWN reason when it named one. A bare `tail -20` of a
  # log another process may also be appending to is how a bind failure got
  # reported as unrelated scheduler warnings.
  wait_log "$lg" '\[httpd\] listening on' "$SERVER_WAIT" \
    || { grep -m1 -a -E '\[httpd\] cannot bind|\[db\] open failed' "$lg" >&2 || tail -20 "$lg"
         die "httpd did not report listening in ${SERVER_WAIT}s"; }
  ok "$(grep -m1 '\[httpd\] listening on' "$lg" | sed 's/^/   /')"
  if [ -n "$JO_NO_SCHED" ]; then warn "scheduler disabled (JO_NO_SCHED set)"
  elif wait_log "$lg" '\[sched\] (background scheduler started|[0-9]+ sources registered)' 8; then
    ok "$(grep -m1 -E '\[sched\] (background scheduler started|[0-9]+ sources registered)' "$lg" | sed 's/^/   /')"
  else warn "no [sched] line yet (scheduler thread may still be spinning up)"; fi
  if wait_http "http://127.0.0.1:$PORT/api/health" 10; then ok "GET /api/health → 200"
  else warn "/api/health not 200 yet (check $lg)"; fi
}

# ---- llama pod -------------------------------------------------------------
cmd_llama(){
  alive llama && { warn "llama pod already running (pid $(cat "$(pidfile llama)"))"; return; }
  [ -x "$LLAMA_BIN" ]   || die "llama-server missing/not executable: $LLAMA_BIN"
  [ -f "$LLAMA_MODEL" ] || die "model missing: $LLAMA_MODEL"
  local lg; lg="$(logfile llama)"; : >"$lg"
  say "llama: $(basename "$LLAMA_BIN")  $(basename "$LLAMA_MODEL")  :$LLAMA_PORT (ctx $LLAMA_CTX)"
  # dylibs sit beside the binary → DYLD_LIBRARY_PATH so dyld finds
  # libllama/libggml*. exec-chain keeps ONE pid (subshell→nohup→env→
  # llama-server, all exec, no fork) so $! is the real llama-server, not a
  # wrapper/subshell. $! is read OUTSIDE the ( )& (precedence-safe).
  local libdir; libdir="$(dirname "$LLAMA_BIN")"
  ( cd "$libdir" && exec nohup env "DYLD_LIBRARY_PATH=$libdir:${DYLD_LIBRARY_PATH:-}" \
      "$LLAMA_BIN" -m "$LLAMA_MODEL" --port "$LLAMA_PORT" --host "$LLAMA_HOST" \
      --ctx-size "$LLAMA_CTX" --jinja --reasoning-format auto -fa auto \
      >>"$lg" 2>&1 ) &
  echo $! >"$(pidfile llama)"
  say "llama: loading $(du -h "$LLAMA_MODEL" 2>/dev/null | cut -f1) model — waiting up to ${LLAMA_WAIT}s for /health"
  if wait_http "http://$LLAMA_HOST:$LLAMA_PORT/health" "$LLAMA_WAIT"; then
    ok "llama /health → 200 (model ready @ http://$LLAMA_HOST:$LLAMA_PORT)"
  else tail -15 "$lg"; die "llama /health not ready in ${LLAMA_WAIT}s (see $lg)"; fi
}

# ---- suggest llama pod (small fast model, /api/search/suggest only) -------
cmd_llama_suggest(){
  alive llama-suggest && { warn "suggest-llama pod already running (pid $(cat "$(pidfile llama-suggest)"))"; return; }
  [ -x "$LLAMA_BIN" ]            || die "llama-server missing/not executable: $LLAMA_BIN"
  [ -f "$LLAMA_SUGGEST_MODEL" ]  || die "suggest model missing: $LLAMA_SUGGEST_MODEL (see plan: download qwen2.5-1.5b-instruct-q4_k_m.gguf into models/)"
  local lg; lg="$(logfile llama-suggest)"; : >"$lg"
  say "suggest-llama: $(basename "$LLAMA_BIN")  $(basename "$LLAMA_SUGGEST_MODEL")  :$LLAMA_SUGGEST_PORT (ctx $LLAMA_SUGGEST_CTX)"
  # same exec-chain/dyld pattern as cmd_llama; --jinja for /v1/chat/completions
  # chat template (osint_suggest uses llm_chat). No --reasoning-format: qwen is
  # a plain instruct model, not a reasoning model.
  local libdir; libdir="$(dirname "$LLAMA_BIN")"
  ( cd "$libdir" && exec nohup env "DYLD_LIBRARY_PATH=$libdir:${DYLD_LIBRARY_PATH:-}" \
      "$LLAMA_BIN" -m "$LLAMA_SUGGEST_MODEL" --port "$LLAMA_SUGGEST_PORT" --host "$LLAMA_HOST" \
      --ctx-size "$LLAMA_SUGGEST_CTX" --jinja -fa auto \
      >>"$lg" 2>&1 ) &
  echo $! >"$(pidfile llama-suggest)"
  say "suggest-llama: loading model — waiting up to ${LLAMA_WAIT}s for /health"
  if wait_http "http://$LLAMA_HOST:$LLAMA_SUGGEST_PORT/health" "$LLAMA_WAIT"; then
    ok "suggest-llama /health → 200 (ready @ http://$LLAMA_HOST:$LLAMA_SUGGEST_PORT)"
  else tail -15 "$lg"; die "suggest-llama /health not ready in ${LLAMA_WAIT}s (see $lg)"; fi
}

# ---- embedding llama pod (optional; hybrid semantic search) ---------------
# Skips (does not die) when the model file is absent: semantic search is an
# addition to the stack, not a precondition of it. JO_EMBED_URL is exported
# only on success so the server pod — which must start AFTER this one for the
# variable to reach it; cmd_up orders it so — sees a URL that answers.
cmd_llama_embed(){
  alive llama-embed && { warn "embed-llama pod already running (pid $(cat "$(pidfile llama-embed)"))"; JO_EMBED_URL="http://$LLAMA_HOST:$LLAMA_EMBED_PORT"; return; }
  [ -x "$LLAMA_BIN" ] || { warn "embed-llama skipped: llama-server missing ($LLAMA_BIN)"; return; }
  [ -f "$LLAMA_EMBED_MODEL" ] || { warn "embed-llama skipped: no embedding model at $LLAMA_EMBED_MODEL (bge-m3 / multilingual-e5 GGUF; set LLAMA_EMBED_MODEL). /api/intel/semantic → 503"; return; }
  local lg; lg="$(logfile llama-embed)"; : >"$lg"
  say "embed-llama: $(basename "$LLAMA_BIN")  $(basename "$LLAMA_EMBED_MODEL")  :$LLAMA_EMBED_PORT (--embedding)"
  local libdir; libdir="$(dirname "$LLAMA_BIN")"
  local pool=(); [ -n "$LLAMA_EMBED_POOLING" ] && pool=( --pooling "$LLAMA_EMBED_POOLING" )
  # --ubatch-size must hold the longest input (non-causal attention embeds the
  # whole prompt in one micro-batch); JO_EMBED_MAX_CHARS default 1000 bytes fits.
  #
  # THE CONTEXT MUST HOLD A WHOLE BATCH, NOT ONE INPUT. It was a hardcoded 2048,
  # which fits one 1000-byte text comfortably and NOT the 32 the embed pod sends
  # per request (JO_EMBED_BATCH): 32 x ~250-500 tokens is 8-16k, so the request
  # sat until core/embed_pod.c's 120 s ceiling and came back `llm_timeout` —
  # measured 2026-09-14, the backfill embedded 1,568 rows and then failed a
  # batch. bge-m3 is trained at 8192, so the ceiling costs nothing but RAM.
  # Keep LLAMA_EMBED_CTX >= JO_EMBED_BATCH x (JO_EMBED_MAX_CHARS / 2) tokens.
  ( cd "$libdir" && exec nohup env "DYLD_LIBRARY_PATH=$libdir:${DYLD_LIBRARY_PATH:-}" \
      "$LLAMA_BIN" -m "$LLAMA_EMBED_MODEL" --port "$LLAMA_EMBED_PORT" --host "$LLAMA_HOST" \
      --embedding --ctx-size "$LLAMA_EMBED_CTX" --batch-size "$LLAMA_EMBED_CTX" \
      --ubatch-size "$LLAMA_EMBED_CTX" "${pool[@]}" \
      >>"$lg" 2>&1 ) &
  echo $! >"$(pidfile llama-embed)"
  say "embed-llama: loading model — waiting up to ${LLAMA_WAIT}s for /health"
  if wait_http "http://$LLAMA_HOST:$LLAMA_EMBED_PORT/health" "$LLAMA_WAIT"; then
    JO_EMBED_URL="http://$LLAMA_HOST:$LLAMA_EMBED_PORT"
    ok "embed-llama /health → 200 (ready @ $JO_EMBED_URL; JO_EMBED_URL passed to the server pod)"
  else tail -15 "$lg"; warn "embed-llama /health not ready in ${LLAMA_WAIT}s (see $lg); JO_EMBED_URL left unset"; fi
}

# ---- web pod (client/ Vite dev server — the site) --------------------------
# bin/japanosint serves only /api; the site is the Vite dev server in client/,
# whose /api proxy forwards to $PORT. Under WSL, client/node_modules is the
# WINDOWS install (esbuild/rollup ship per-OS binaries) and there is no Linux
# node, so the pod runs node.exe over interop — detached through
# Start-Process, because a Windows child still tied to this shell's pipes dies
# on EPIPE once the WSL side goes away. networkingMode=mirrored makes
# 127.0.0.1 the same host on both sides, so wait_http works unchanged.
# Liveness is "who owns $WEB_PORT", not a pidfile: the WSL-side $! of an
# interop launch is not the Windows node pid, so a pidfile would lie. Every
# interop call takes </dev/null: a Windows exe inherits and DRAINS the
# caller's stdin, which silently ate the rest of a piped/`bash -s` script.
web_is_windows(){
  case "$WEB_RUNTIME" in windows) return 0 ;; native) return 1 ;; esac
  grep -qi microsoft /proc/version 2>/dev/null && command -v node.exe >/dev/null 2>&1 \
    && case "$REPO" in /mnt/*) true ;; *) false ;; esac
}
# The listener probe. It ALWAYS prints one line and ALWAYS exits 0, so the
# three outcomes stay distinguishable from the shell:
#
#   "NONE"              the port is free
#   "<pid> <cmdline>"   that process holds it
#   nothing / rc != 0   the question could not be asked at all
#
# The previous form let PowerShell's own exit status through, and
# `Get-NetTCPConnection` finding nothing exits 1 — so "free" and "broken" were
# the same answer, in opposite directions depending on which the caller assumed.
WEB_OWNER_PS='$c = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
if ($c) { $p = Get-CimInstance Win32_Process -Filter ("ProcessId=" + $c.OwningProcess) -ErrorAction SilentlyContinue
          "" + $c.OwningProcess + " " + $p.CommandLine } else { "NONE" }
exit 0'

# ── WSL interop: detect, and REPAIR rather than lecture ────────────────────
#
# Every Windows-side operation here (start Vite, find who holds the port, stop
# it) runs a .exe from Linux, which works only while the kernel has a binfmt
# handler registered for PE binaries. On this machine that registration
# DISAPPEARS intermittently: /proc/sys/fs/binfmt_misc/WSLInterop goes missing,
# every .exe dies with "Exec format error" (exit 126), and the script used to
# hand the operator a four-line paste-this-into-Windows instruction for
# something the machine can fix itself.
#
# It can be re-registered — the entry is plain text and this is exactly what
# WSL itself writes at boot. Confirmed against a healthy distro on the same
# machine: name WSLInterop, magic MZ (4d5a) at offset 0, interpreter /init,
# flag P. Writing to .../register needs root, so this asks for sudo ONCE, in
# an interactive shell, and says why. Non-interactive (CI, a pipe) never
# prompts — it reports and lets the caller decide.
# PRESENT IS NOT THE SAME AS ENABLED. binfmt entries can be switched off
# (echo 0 > .../WSLInterop) and the file stays, so testing for existence alone
# would call a dead handler healthy and skip the repair.
interop_ok(){
  [ -e /proc/sys/fs/binfmt_misc/WSLInterop ] || return 1
  grep -qx enabled /proc/sys/fs/binfmt_misc/WSLInterop 2>/dev/null
}

# CAN WE ASK THE HUMAN FOR A PASSWORD?
#
# `[ -t 0 ]` answers "is STDIN a terminal", which is the wrong question and
# was too strict: a person running `./launch.sh | tee log`, or any of this
# script's own `cmd < /dev/null` calls, has stdin redirected while a terminal
# is still right there. The right question is whether a CONTROLLING TERMINAL
# exists, and /dev/tty is exactly that — it is the terminal the process is
# attached to regardless of where stdin points. sudo reads its prompt from
# /dev/tty for the same reason, so pointing it there makes the prompt work in
# every case where a human is actually present, and fail cleanly (no hang) in
# cron, CI and pipelines where /dev/tty cannot be opened.
have_tty(){ [ -e /dev/tty ] && { : < /dev/tty; } 2>/dev/null; }

# sudo with its prompt wired to the terminal rather than to our stdin.
sudo_tty(){ sudo -p "[launch] %p's password (to re-register WSL interop): " "$@" < /dev/tty; }

interop_repair(){
  # EVERY MESSAGE IN HERE GOES TO STDERR.
  #
  # This is called from web_owner(), whose stdout IS ITS RETURN VALUE
  # (`o="$(web_owner)"`). The first version printed its progress with the
  # ordinary say/ok/warn helpers, which write to stdout — so the repair notice
  # was captured AS THE PORT OWNER, and web_stop then reported
  # `stopped web pod (vite pid <ansi escape> on :5173)` for a pod it had not
  # touched. A false success is worse than the failure it replaced.
  exec 3>&1 1>&2                         # stdout → stderr for this function
  _ir_done(){ exec 1>&3 3>&-; }          # restore before every return
  grep -qi microsoft /proc/version 2>/dev/null || { _ir_done; return 1; }
  interop_ok && { _ir_done; return 0; }
  [ -w /proc/sys/fs/binfmt_misc/register ] || command -v sudo >/dev/null 2>&1 || {
    warn "WSL interop is unregistered and sudo is unavailable — cannot repair"; _ir_done; return 1; }

  # Present but switched off: just switch it back on, which is cheaper than a
  # re-register and is the only thing that works while the name is taken.
  local f=/proc/sys/fs/binfmt_misc/WSLInterop
  if [ -e "$f" ]; then
    if [ -w "$f" ]; then echo 1 > "$f" 2>/dev/null
    elif sudo -n true 2>/dev/null; then echo 1 | sudo tee "$f" >/dev/null 2>&1
    elif have_tty; then
      say "WSL interop is registered but DISABLED — re-enabling it needs root."
      echo 1 | sudo_tty tee "$f" >/dev/null
    fi
    interop_ok && { ok "WSL interop re-enabled"; _ir_done; return 0; }
  fi

  local reg=':WSLInterop:M::MZ::/init:P'
  if [ -w /proc/sys/fs/binfmt_misc/register ]; then          # already root
    printf '%s' "$reg" > /proc/sys/fs/binfmt_misc/register 2>/dev/null
  elif sudo -n true 2>/dev/null; then                        # passwordless
    printf '%s' "$reg" | sudo tee /proc/sys/fs/binfmt_misc/register >/dev/null 2>&1
  elif have_tty; then                                        # ask, once
    say "WSL interop (running Windows .exe from Linux) is unregistered — this is
     why the web pod cannot be started or stopped. Re-registering it needs
     root; sudo will ask for your password. Ctrl-C to skip."
    printf '%s' "$reg" | sudo_tty tee /proc/sys/fs/binfmt_misc/register >/dev/null
  else
    warn "WSL interop is unregistered and no terminal is available to ask for a
     sudo password (no /dev/tty). Run ./launch.sh from a terminal, or fix it
     once with: wsl.exe --shutdown"
    _ir_done; return 1
  fi

  if interop_ok; then ok "WSL interop re-registered"; _ir_done; return 0; fi
  warn "could not re-register WSL interop — 'wsl.exe --shutdown' then reopen the shell"
  _ir_done; return 1
}

# IS THE DEV SERVER ANSWERING? — the one question that needs no interop.
#
# Everything else here asks WINDOWS who owns the port, which needs
# powershell.exe, which is exactly what breaks. Measured 2026-09-13: in a
# long-running WSL shell `powershell.exe` exited 126 ("found, could not
# execute") on every call, while a freshly started session in the same distro
# ran it fine — so the pod could be perfectly healthy and up, and launch.sh
# would refuse to say so. An HTTP GET answers "is it up" from inside WSL with
# no interop at all, and Vite's dev server identifies itself in the body
# (/@react-refresh or /@vite/client are injected into every page it serves).
#
# Returns 0 = Vite is answering, 1 = something else is, 2 = nothing is.
web_http_probe(){
  local body
  body="$(curl -fsS --max-time 3 "http://127.0.0.1:$WEB_PORT/" 2>/dev/null)" || return 2
  case "$body" in *"/@vite/client"*|*"/@react-refresh"*|*"type=\"module\""*) return 0 ;; esac
  return 1
}

# "PID COMMANDLINE" of whatever listens on $WEB_PORT, "? probe-failed …" when
# the question could not be asked, or nothing when the port is genuinely free.
web_owner(){
  local pid=""
  if web_is_windows; then
    # "NOT RUNNING" AND "COULD NOT TELL" MUST NOT LOOK THE SAME.
    #
    # This swallowed powershell's exit status, so a probe that failed to run at
    # all — interop hiccup, PowerShell startup starved behind a `make -j12`,
    # WMI refusing the CIM query — was indistinguishable from an empty port.
    # Every caller then believed the pod was stopped: freeze skipped it, and
    # cmd_web walked into a port that was already taken and died on the far
    # side with "Start-Process via powershell.exe failed". Seen 2026-09-13 with
    # a Vite from 00:17 still listening. A failed probe now says so, and the
    # callers refuse rather than guess.
    local out rc; rc=0
    out="$(powershell.exe -NoProfile -NonInteractive -Command \
            "\$port=$WEB_PORT; $WEB_OWNER_PS" </dev/null 2>/dev/null | tr -d '\r' | head -1)" || rc=$?
    # 126/127 == the shell could not run a .exe at all. That is repairable
    # (see interop_repair); try once, then ask again, before giving up and
    # falling back to the HTTP probe below.
    if { [ "$rc" = 126 ] || [ "$rc" = 127 ]; } && interop_repair; then
      rc=0
      out="$(powershell.exe -NoProfile -NonInteractive -Command \
              "\$port=$WEB_PORT; $WEB_OWNER_PS" </dev/null 2>/dev/null | tr -d '\r' | head -1)" || rc=$?
    fi
    if [ $rc -ne 0 ] || [ -z "$out" ]; then
      # The pid is unknowable without interop, but "is it up" is not: ask the
      # port directly rather than reporting a shrug. Only when NOTHING answers
      # is it safe to call the port free.
      if web_http_probe; then
        printf '? vite is answering on :%s (pid unknown: powershell rc=%d)\n' "$WEB_PORT" "$rc"
      elif [ $? -eq 1 ]; then
        printf '? something is answering on :%s, not Vite (pid unknown: powershell rc=%d)\n' "$WEB_PORT" "$rc"
      fi
      return 0                                     # nothing answering → free
    fi
    [ "$out" = "NONE" ] || printf '%s\n' "$out"      # NONE == the port is free
    return 0
  fi
  if command -v lsof >/dev/null 2>&1; then
    pid=$(lsof -tiTCP:"$WEB_PORT" -sTCP:LISTEN 2>/dev/null | head -1 || true)
  elif command -v ss >/dev/null 2>&1; then
    pid=$(ss -ltnp 2>/dev/null | grep ":$WEB_PORT " | grep -oE 'pid=[0-9]+' | head -1 | cut -d= -f2 || true)
  fi
  [ -z "$pid" ] || printf '%s %s\n' "$pid" "$(ps -o command= -p "$pid" 2>/dev/null || true)"
}
# stop the Vite on $WEB_PORT — only if it IS Vite; a foreign listener is left alone
web_stop(){
  local o pid; o="$(web_owner)"
  # ORDER MATTERS: the "could not identify the owner" sentinel is
  # "? vite is answering on :N (pid unknown…)", which contains the word vite
  # and so matched a *vite* pattern placed first — the script then tried to
  # kill a pid of "?" and reported success for a pod that was still serving.
  # A false "stopped" is the worst outcome available here, so the sentinel is
  # tested before anything else and success is asserted, never assumed.
  case "$o" in
    '? '*)
      if interop_repair; then o="$(web_owner)"; fi   # repaired → real answer
      case "$o" in
        '? '*) warn "web: $o
     Interop could not be repaired, so this shell cannot stop a WINDOWS
     process. From Windows:
       powershell -Command \"Get-NetTCPConnection -LocalPort $WEB_PORT -State Listen |
         ForEach-Object { Stop-Process -Id \$_.OwningProcess -Force }\""
          return 0 ;;
        *vite*) ;;                                   # repaired: stop it below
        *) return 0 ;;
      esac ;;
    *vite*) ;;
    *) return 0 ;;
  esac
  pid="${o%% *}"
  case "$pid" in ''|*[!0-9]*) warn "web: owner line has no usable pid ($o)"; return 0 ;; esac
  if web_is_windows; then powershell.exe -NoProfile -NonInteractive -Command "Stop-Process -Id $pid -Force" </dev/null >/dev/null 2>&1 || true
  else kill "$pid" 2>/dev/null || true; fi
  local i
  for i in $(seq 1 10); do o="$(web_owner)"; [ -z "$o" ] && break; sleep 0.5; done
  if [ -z "$o" ]; then ok "stopped web pod (vite pid $pid on :$WEB_PORT)"
  else warn "web: pid $pid did not stop — :$WEB_PORT is still held by → $o"; fi
}
cmd_web(){
  local cdir="$REPO/client" lg o; lg="$(logfile web)"
  [ -f "$cdir/package.json" ] || die "no client/package.json under $REPO"
  o="$(web_owner)"
  if [ -n "$o" ]; then
    case "$o" in
      '? vite is answering'*)
        # The pod IS up; only its pid is unknowable from this shell. Reporting
        # that as a failure (which it was) told the operator to fix a server
        # that was already serving.
        ok "web pod already running → http://$WEB_HOST:$WEB_PORT/  ($o)"
        return ;;
      '? '*)
        # Something holds the port and it is not us. Starting a second Vite on
        # it is how this ends as an opaque Start-Process failure.
        die "web: :$WEB_PORT is occupied and this shell cannot identify the owner.
     $o
     Check from Windows:  powershell -Command \"Get-NetTCPConnection -LocalPort $WEB_PORT -State Listen\"" ;;
      *vite*) warn "web pod already running (pid ${o%% *}) → http://$WEB_HOST:$WEB_PORT/"; return ;;
    esac
    die "port $WEB_PORT is held by something that is not Vite: $o"
  fi
  if [ ! -f "$cdir/node_modules/vite/bin/vite.js" ]; then
    local inst=install; [ -f "$cdir/package-lock.json" ] && inst=ci
    say "web: client/node_modules missing — npm $inst (log $(logfile web-install))"
    if web_is_windows; then ( cd "$cdir" && cmd.exe /c npm "$inst" ) </dev/null >"$(logfile web-install)" 2>&1
    else command -v npm >/dev/null || die "npm not found — install Node.js"
         ( cd "$cdir" && npm "$inst" ) >"$(logfile web-install)" 2>&1; fi \
      || { tail -15 "$(logfile web-install)"; die "web: npm $inst failed"; }
  fi
  # TRUNCATING THE LOG CAN FAIL, AND IT MEANS SOMETHING.
  #
  # Under WSL the previous Vite is a WINDOWS process whose `cmd /c … > web.log`
  # parent holds this file open with a share mode that denies writers, so
  # `: > web.log` comes back EACCES. That happened on 2026-09-13: a Vite from
  # 00:17 was still listening on :5173 with its cmd parent holding the log, and
  # the run failed twice over — "Permission denied" here, then an opaque
  # "Start-Process via powershell.exe failed" when the new server could not
  # take the port. Neither message named the cause. A locked log is not a
  # detail to step over; it is evidence that a pod we think is stopped is
  # still running.
  if ! : >"$lg" 2>/dev/null; then
    local holder; holder="$(web_owner)"
    if [ -n "$holder" ]; then
      die "web: $lg is locked, and :$WEB_PORT is held by → $holder
     A previous web pod is still alive. Stop it with:  ./launch.sh web-stop"
    fi
    die "web: cannot write $lg (locked by another process, or the directory is
     read-only). Under WSL the usual holder is a Vite started earlier — possibly
     on a DIFFERENT port, since every web pod shares this one log file. Try
     ./launch.sh web-stop, or set RUN=<dir> to log elsewhere."
  fi
  say "web: vite → http://$WEB_HOST:$WEB_PORT/  (/api → http://127.0.0.1:$PORT)"
  if web_is_windows; then
    local wdir wlog; wdir="$(wslpath -w "$cdir")"; wlog="$(wslpath -w "$lg")"
    # Keep powershell's own error text: `2>/dev/null || die "…failed"` threw
    # away the one sentence that says WHY, and left the operator with a
    # four-word failure for anything from a busy port to broken interop.
    # Starting a Windows Vite needs interop by definition — repair it first
    # rather than failing and telling the operator to do it by hand.
    interop_ok || interop_repair || true
    local perr rc; perr="$(logfile web-start)"; rc=0
    # `cmd; rc=$?` would be a bug here: this script runs under `set -e`, so the
    # failing command exits the shell BEFORE the next line assigns rc, and the
    # careful error handling below never runs — the operator gets a silent
    # exit 126 with no message at all. `|| rc=$?` both protects and captures.
    powershell.exe -NoProfile -NonInteractive -Command \
      "\$env:JO_API_TARGET='http://127.0.0.1:$PORT'; Start-Process -WindowStyle Hidden -WorkingDirectory '$wdir' cmd -ArgumentList '/c','node node_modules/vite/bin/vite.js --host $WEB_HOST --port $WEB_PORT --strictPort > \"$wlog\" 2>&1'" \
      </dev/null >"$perr" 2>&1 || rc=$?
    if [ "$rc" -ne 0 ]; then
      sed -n '1,12p' "$perr" >&2
      # 126/127 are not "PowerShell said no" — they are "the shell could not
      # run a Windows binary at all". That is WSL interop, and it is a property
      # of the SESSION: measured 2026-09-13, a long-running shell returned 126
      # for every powershell.exe call while a freshly started one in the same
      # distro worked. Saying "Start-Process failed" sends the operator to look
      # at Vite, which is the one thing that is not wrong.
      case $rc in
        126|127) die "web: cannot execute powershell.exe (exit $rc) — the WSL binfmt
     handler for Windows binaries is unregistered and could not be repaired
     automatically. Fix with:  wsl.exe --shutdown  (then reopen this shell),
     or run this script from a terminal so it can sudo-register it.
     (A native-node web pod needs node inside WSL: WEB_RUNTIME=native.)" ;;
        *) die "web: Start-Process via powershell.exe failed (exit $rc, see $perr).
     Common causes: a previous Vite still on :$WEB_PORT (./launch.sh web-stop),
     or node.exe missing on the WINDOWS PATH." ;;
      esac
    fi
  else
    command -v node >/dev/null || die "node not found (install Node.js, or WEB_RUNTIME=windows under WSL)"
    ( cd "$cdir" && JO_API_TARGET="http://127.0.0.1:$PORT" exec nohup node node_modules/vite/bin/vite.js \
        --host "$WEB_HOST" --port "$WEB_PORT" --strictPort </dev/null >>"$lg" 2>&1 ) &
  fi
  wait_http "http://127.0.0.1:$WEB_PORT/" "$WEB_WAIT" \
    || { tail -20 "$lg"; die "web: vite did not answer on :$WEB_PORT in ${WEB_WAIT}s (see $lg)"; }
  ok "web → http://$WEB_HOST:$WEB_PORT/  (log $lg)"
  if wait_http "http://127.0.0.1:$WEB_PORT/api/health" 5; then ok "web /api proxy → :$PORT /api/health 200"
  else warn "site is up but /api/health through its proxy is not 200 (server pod down?)"; fi
}

# ---- maintenance / enrich pod (one-shot) ----------------------------------
cmd_maint(){
  [ -x "$BIN" ] || die "no bin/japanosint — build first"
  [ "$LLM_ENABLED" = "true" ] || warn "LLM_ENABLED!=true → enricher will skip (Node parity)"
  wait_http "http://127.0.0.1:$LLAMA_PORT/health" 5 || warn "llama /health not up — NER per-item will fail-soft"
  say "maint: bin/japanosint --run llm-enricher (one-shot graph warm)"
  JO_DB="$JO_DB" JO_ENV_FILE="$JO_ENV_FILE" LLM_ENABLED="$LLM_ENABLED" \
  LLM_BASE_URL="$LLM_BASE_URL" "$BIN" --run llm-enricher 2>&1 | tee -a "$(logfile maint)" | tail -8
  ok "maint pass complete (ongoing enrichment continues in the server scheduler)"
}

# ---- status / down / logs --------------------------------------------------
cmd_status(){
  say "status"
  for p in server llama llama-suggest llama-embed; do
    if alive "$p"; then ok "$p pod  RUNNING  pid=$(cat "$(pidfile "$p")")  log=$(logfile "$p")"
    else warn "$p pod  stopped"; fi
  done
  local wo; wo="$(web_owner)"
  case "$wo" in *vite*) ok "web pod  RUNNING  pid=${wo%% *}  http://$WEB_HOST:$WEB_PORT/  log=$(logfile web)" ;;
                    *) warn "web pod  stopped${wo:+ (:$WEB_PORT held by: $wo)}" ;; esac
  curl -fsS -m3 "http://127.0.0.1:$PORT/api/health"            2>/dev/null && echo "  ← :$PORT /api/health" || warn "server :$PORT not answering /api/health"
  curl -fsS -m3 "http://127.0.0.1:$LLAMA_PORT/health"          2>/dev/null && echo "  ← :$LLAMA_PORT llama /health" || warn "llama :$LLAMA_PORT not answering /health"
  curl -fsS -m3 "http://127.0.0.1:$LLAMA_SUGGEST_PORT/health"  2>/dev/null && echo "  ← :$LLAMA_SUGGEST_PORT suggest-llama /health" || warn "suggest-llama :$LLAMA_SUGGEST_PORT not answering /health"
  curl -fsS -m3 "http://127.0.0.1:$LLAMA_EMBED_PORT/health"    2>/dev/null && echo "  ← :$LLAMA_EMBED_PORT embed-llama /health" || warn "embed-llama :$LLAMA_EMBED_PORT not answering /health (optional; /api/intel/semantic → 503)"
}
cmd_down(){ cmd_freeze; }
cmd_logs(){ local w="${1:-server}"; tail -n 60 -f "$(logfile "$w")"; }

# ---- orchestrate -----------------------------------------------------------
cmd_up(){
  cmd_freeze
  [ "$DO_BUILD"  = 1 ] && cmd_build  || say "build skipped (--no-build)"
  # embed pod BEFORE the server: cmd_serve snapshots JO_EMBED_URL into BINENV.
  [ "$DO_EMBED_LLAMA" = 1 ] && cmd_llama_embed || say "embed-llama skipped (--no-embed-llama)"
  [ "$DO_SERVER" = 1 ] && cmd_serve  || say "server skipped (--no-server)"
  [ "$DO_WEB"    = 1 ] && cmd_web    || say "web skipped (--no-web)"
  [ "$DO_LLAMA"  = 1 ] && cmd_llama  || say "llama skipped (--no-llama)"
  [ "$DO_SUGGEST_LLAMA" = 1 ] && cmd_llama_suggest || say "suggest-llama skipped (--no-suggest-llama)"
  [ "$DO_MAINT"  = 1 ] && cmd_maint  || true
  cmd_status
  [ "$DO_WEB" = 1 ] && say "site: http://$WEB_HOST:$WEB_PORT/"
  say "up. logs: $RUN/{server,web,llama,llama-suggest}.log   stop: ./launch.sh down   tags: ./launch.sh tags"
}

case "$CMD" in
  up)           cmd_up ;;
  build)        cmd_freeze; cmd_build ;;
  serve|server) cmd_serve ;;
  web)          cmd_web ;;
  web-stop)     web_stop ;;
  llama)        cmd_llama ;;
  llama-suggest) cmd_llama_suggest ;;
  llama-embed)  cmd_llama_embed ;;
  maint)        cmd_maint ;;
  status)       cmd_status ;;
  down|stop)    cmd_down ;;
  logs)         cmd_logs "${1:-server}" ;;
  freeze)       cmd_freeze ;;
  tags|help)    cmd_tags ;;
  *) die "unknown command '$CMD' (up|build|serve|web|web-stop|llama|maint|status|down|logs|tags)";;
esac
