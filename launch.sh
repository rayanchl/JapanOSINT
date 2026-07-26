#!/usr/bin/env bash
# ============================================================================
# JapanOSINT — native backend launch orchestrator
# ----------------------------------------------------------------------------
# Pods (independently toggleable):
#   build    cd native && make  (INCREMENTAL — only rebuilds changed objects)
#   server   ./bin/japanosint --serve   (httpd + background scheduler+enricher)
#   llama    native/llama/llama-server  (gpt-oss-20b, :8080)
#   maint    one-shot `--run llm-enricher` to warm the entity graph (optional;
#            ongoing enrichment already runs inside the server scheduler)
#
# Usage:
#   ./launch.sh up              freeze → build(if needed) → server → llama → status
#   ./launch.sh up --maint      …also kick the maintenance/enricher pod once
#   ./launch.sh build [--rebuild]
#   ./launch.sh serve | llama | maint | status | down | logs [server|llama]
#   ./launch.sh tags            print EVERY launch tag/env knob (grouped)
#
# Pod skips / knobs (any subset):  --no-build --no-server --no-llama --maint
#   --rebuild  --force  --port N  --db PATH  --env-file PATH  --model PATH
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
RUN="$NATIVE/.run"            # pidfiles + logs (gitignore native/.run/)
mkdir -p "$RUN"

# ---- launch tags (env wins → flags override → these prod defaults) ---------
# Runtime / server pod
: "${PORT:=4000}"                                   # httpd port
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
: "${LLAMA_MODEL:=$REPO/models/gpt-oss-20b-Q4_K_M.gguf}"
: "${LLAMA_PORT:=8080}"
: "${LLAMA_CTX:=16384}"
: "${LLAMA_HOST:=127.0.0.1}"
# Suggest LLM pod — dedicated small/fast model for /api/search/suggest so it
# never contends with the pipeline's 20B (falls back to LLM_BASE_URL in-binary)
: "${LLM_SUGGEST_BASE_URL:=http://127.0.0.1:8081}"  # binary reads this for suggest
: "${LLAMA_SUGGEST_MODEL:=$REPO/models/qwen2.5-1.5b-instruct-q4_k_m.gguf}"
: "${LLAMA_SUGGEST_PORT:=8081}"
: "${LLAMA_SUGGEST_CTX:=4096}"
# Orchestrator behaviour
: "${EXPECT_SOURCES:=476}"                          # warn if registry differs
: "${LLAMA_WAIT:=240}"                              # s to wait for model load
: "${SERVER_WAIT:=30}"                              # s to wait for httpd listen

DO_BUILD=1 DO_SERVER=1 DO_LLAMA=1 DO_SUGGEST_LLAMA=1 DO_MAINT=0 REBUILD=0 FORCE=0

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
  --maint)     DO_MAINT=1 ;;
  --rebuild)   REBUILD=1 ;;
  --force)     FORCE=1 ;;
  --port)      PORT="$2"; shift ;;
  --db)        JO_DB="$2"; shift ;;
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
  [ -f "$JO_ENV_FILE" ] && sed -nE "s/^[[:space:]]*$k=[\"']?([^\"'#]*)[\"']?.*/\1/p" "$JO_ENV_FILE" | head -1; }
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
  [ -n "${LLM_MODEL:-}" ]          && BINENV+=( "LLM_MODEL=$LLM_MODEL" ); }

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
  --maint            after server is up, run llm-enricher once (warm graph)
  --rebuild          make clean && make (full)        --force  skip make-guard
  --port N  --db PATH  --env-file PATH  --model PATH

${G}SERVER / RUNTIME POD  (read by bin/japanosint)${NC}
  PORT=$PORT                 httpd port            (binary default 4000)
  JO_DB=$JO_DB
  JO_SCHEMA=...schema.sql      auto-applied on open
  JO_ENV_FILE=$JO_ENV_FILE   binary loads it itself
  JO_NO_SCHED=${JO_NO_SCHED:-<unset>}            set→serve WITHOUT scheduler/enricher
  CLI: --serve  --run <id> [entity]  --list-sources  --sched  --wakati  (none=selftest)

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
  for p in server llama llama-suggest; do
    if alive "$p"; then kill "$(cat "$(pidfile "$p")")" 2>/dev/null && ok "stopped $p pod"; fi
    rm -f "$(pidfile "$p")"
  done
  # stragglers not tracked by a pidfile
  pkill -f "$BIN --serve"                2>/dev/null && ok "killed stray japanosint --serve" || true
  pkill -f "llama-server.*gpt-oss-20b"   2>/dev/null && ok "killed stray llama-server"        || true
  pkill -f "llama-server.*qwen2.5-1.5b"  2>/dev/null && ok "killed stray suggest llama-server" || true
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
  if ! make -C "$NATIVE" -j8 2>&1 | tee "$(logfile build)" \
       | grep -iE 'error:|undefined reference|fatal' \
       | grep -viE 'deprecated|SHA1|MD5|RSA_|EC_KEY|ECDSA_|EVP_PKEY_assign'; then :; fi
  [ -x "$BIN" ] || die "build failed — no bin/japanosint (see $(logfile build))"
  cmd_build_verify
}
cmd_build_verify(){
  local n; n=$("$BIN" --list-sources 2>/dev/null | wc -l | tr -d ' ')
  if [ "$n" = "$EXPECT_SOURCES" ]; then ok "build green — $n sources registered"
  else warn "build green but $n sources registered (expected $EXPECT_SOURCES — may be OK if registry changed)"; fi
}

# ---- server pod ------------------------------------------------------------
cmd_serve(){
  [ -x "$BIN" ] || die "no bin/japanosint — run: ./launch.sh build"
  alive server && { warn "server pod already running (pid $(cat "$(pidfile server)"))"; return; }
  if [ -n "$(envget SUPABASE_URL)" ]; then ok "auth: SUPABASE_URL present (via ${SUPABASE_URL:+env}${SUPABASE_URL:-$JO_ENV_FILE})"
  else warn "SUPABASE_URL not in env or $JO_ENV_FILE — prod RS256/JWKS off (HS256/dev only)"; fi
  local lg; lg="$(logfile server)"; : >"$lg"
  say "server: bin/japanosint --serve  (PORT=$PORT DB=$JO_DB LLM_ENABLED=$LLM_ENABLED)"
  binenv
  nohup env "${BINENV[@]}" "$BIN" --serve >>"$lg" 2>&1 &
  echo $! >"$(pidfile server)"
  wait_log "$lg" '\[httpd\] listening on' "$SERVER_WAIT" \
    || { tail -20 "$lg"; die "httpd did not report listening in ${SERVER_WAIT}s"; }
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
  say "llama: loading 11G model — waiting up to ${LLAMA_WAIT}s for /health"
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
  for p in server llama llama-suggest; do
    if alive "$p"; then ok "$p pod  RUNNING  pid=$(cat "$(pidfile "$p")")  log=$(logfile "$p")"
    else warn "$p pod  stopped"; fi
  done
  curl -fsS -m3 "http://127.0.0.1:$PORT/api/health"            2>/dev/null && echo "  ← :$PORT /api/health" || warn "server :$PORT not answering /api/health"
  curl -fsS -m3 "http://127.0.0.1:$LLAMA_PORT/health"          2>/dev/null && echo "  ← :$LLAMA_PORT llama /health" || warn "llama :$LLAMA_PORT not answering /health"
  curl -fsS -m3 "http://127.0.0.1:$LLAMA_SUGGEST_PORT/health"  2>/dev/null && echo "  ← :$LLAMA_SUGGEST_PORT suggest-llama /health" || warn "suggest-llama :$LLAMA_SUGGEST_PORT not answering /health"
}
cmd_down(){ cmd_freeze; }
cmd_logs(){ local w="${1:-server}"; tail -n 60 -f "$(logfile "$w")"; }

# ---- orchestrate -----------------------------------------------------------
cmd_up(){
  cmd_freeze
  [ "$DO_BUILD"  = 1 ] && cmd_build  || say "build skipped (--no-build)"
  [ "$DO_SERVER" = 1 ] && cmd_serve  || say "server skipped (--no-server)"
  [ "$DO_LLAMA"  = 1 ] && cmd_llama  || say "llama skipped (--no-llama)"
  [ "$DO_SUGGEST_LLAMA" = 1 ] && cmd_llama_suggest || say "suggest-llama skipped (--no-suggest-llama)"
  [ "$DO_MAINT"  = 1 ] && cmd_maint  || true
  cmd_status
  say "up. logs: $RUN/{server,llama,llama-suggest}.log   stop: ./launch.sh down   tags: ./launch.sh tags"
}

case "$CMD" in
  up)           cmd_up ;;
  build)        cmd_freeze; cmd_build ;;
  serve|server) cmd_serve ;;
  llama)        cmd_llama ;;
  llama-suggest) cmd_llama_suggest ;;
  maint)        cmd_maint ;;
  status)       cmd_status ;;
  down|stop)    cmd_down ;;
  logs)         cmd_logs "${1:-server}" ;;
  freeze)       cmd_freeze ;;
  tags|help)    cmd_tags ;;
  *) die "unknown command '$CMD' (up|build|serve|llama|maint|status|down|logs|tags)";;
esac
