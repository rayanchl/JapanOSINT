#!/bin/bash
# bench_concurrency.sh — does a slow route stop the whole SERVER?
#
#   tools/bench_concurrency_setup.sh     # isolated server on :4055 + a token
#   tools/bench_concurrency.sh           # ~6 minutes, prints a table
#
# Measured 2026-09-11 (129 MB db, 16,367 sources, scheduler running) and again
# on 2026-09-12 after the fixes in docs/concurrency-plan-2026-09-11.md §5b:
#
#                              health MAX before   after
#   5 x /api/data/<layer>          0.0017 s      0.0018 s  (already off-loop)
#   10 x /api/intel/items          0.038 s       0.0015 s
#   3 x /api/intel/sources         7.8 s         0.008 s
#   3 x /api/status                24.8 s        0.004 s
#   mixed, everything at once      7.4 s         0.0013 s
#
# The gate that keeps it that way is tools/ci_concurrency_gate.sh (no network,
# runs in CI). This harness is the fuller picture, for when you are changing
# something and want to see all five shapes.
#
# Concurrency measurement against the isolated server on :4055.
#
# The question is not "how fast is route X" but "does route X stop the SERVER".
# Every scenario runs a ~10 Hz probe against /api/health — pre-auth, constant
# work, ~1 ms when the event loop is free — and reports the probe's worst
# latency. A probe max that tracks the load's own duration means the loop was
# blocked; a flat probe means the handler really did move off it.
set -u
B=http://127.0.0.1:4055
W=$HOME/conctest
T=$(cat "$W/token")
R=$W/results; rm -rf "$R"; mkdir -p "$R"

probe() {                       # $1 = tag, $2 = seconds
  local tag=$1
  local secs=$2
  local end=$(( $(date +%s) + secs ))
  : > "$R/probe_$tag.txt"
  while [ "$(date +%s)" -lt "$end" ]; do
    curl -sS --max-time 30 -o /dev/null -w '%{time_total} %{http_code}\n' \
      "$B/api/health" >> "$R/probe_$tag.txt" 2>/dev/null || echo "30.000 000" >> "$R/probe_$tag.txt"
    sleep 0.1
  done
}

stats() {                       # $1 = probe file
  local f=$1
  local n p50 p95 mx bad
  n=$(wc -l < "$f")
  [ "$n" -eq 0 ] && { echo "n=0"; return; }
  p50=$(cut -d' ' -f1 "$f" | sort -n | awk -v n="$n" 'NR==int(n*0.5)+1{print;exit}')
  p95=$(cut -d' ' -f1 "$f" | sort -n | awk -v n="$n" 'NR==int(n*0.95)+1{print;exit}')
  mx=$(cut -d' ' -f1 "$f" | sort -n | tail -1)
  bad=$(awk '$2!="200"' "$f" | wc -l)
  echo "n=$n p50=${p50}s p95=${p95}s MAX=${mx}s non200=$bad"
}

hit() {                         # $1 = tag, $2 = path (background)
  local tag=$1 path=$2
  ( curl -sS --max-time 300 -o /dev/null \
      -H "Authorization: Bearer $T" \
      -w "$tag %{http_code} %{size_download} %{time_total}\n" \
      "$B$path" >> "$R/load.txt" 2>/dev/null \
    || echo "$tag FAIL 0 300" >> "$R/load.txt" ) &
}

show_load() { sort "$R/load.txt" | awk '{printf "    %-14s status=%-4s bytes=%-9s t=%ss\n",$1,$2,$3,$4}'; }

echo "### 0. baseline — no load"
probe idle 8
printf '  health          : '; stats "$R/probe_idle.txt"

echo
echo "### 1. ten concurrent /api/intel/items?limit=200 (DB reads)"
: > "$R/load.txt"
probe intel 20 & PB=$!
for i in $(seq 1 10); do hit intel "/api/intel/items?limit=200"; done
wait $PB; wait
printf '  health during   : '; stats "$R/probe_intel.txt"
echo   "  load requests   :"; show_load | head -4

echo
echo "### 2. three concurrent /api/status (the 20 MB route)"
: > "$R/load.txt"
probe status 60 & PB=$!
for i in 1 2 3; do hit status "/api/status"; done
wait $PB; wait
printf '  health during   : '; stats "$R/probe_status.txt"
echo   "  load requests   :"; show_load

echo
echo "### 3. three concurrent /api/intel/sources (~10 MB aggregate)"
: > "$R/load.txt"
probe srcs 60 & PB=$!
for i in 1 2 3; do hit srcs "/api/intel/sources"; done
wait $PB; wait
printf '  health during   : '; stats "$R/probe_srcs.txt"
echo   "  load requests   :"; show_load

echo
echo "### 4. five concurrent /api/data/<layer> (collector-running route)"
: > "$R/load.txt"
probe data 120 & PB=$!
for l in castles museums aed-map shelters cameras; do hit "data:$l" "/api/data/$l"; done
wait $PB; wait
printf '  health during   : '; stats "$R/probe_data.txt"
echo   "  load requests   :"; show_load

echo
echo "### 5. mixed: everything at once"
: > "$R/load.txt"
probe mixed 120 & PB=$!
for i in $(seq 1 6); do hit intel "/api/intel/items?limit=200"; done
for i in 1 2; do hit status "/api/status"; done
for l in castles aed-map shelters; do hit "data:$l" "/api/data/$l"; done
hit semantic "/api/intel/semantic?q=earthquake&limit=10"
hit entities "/api/entities?limit=100"
wait $PB; wait
printf '  health during   : '; stats "$R/probe_mixed.txt"
echo   "  load requests   :"; show_load

echo
echo "### scheduler ran throughout (never paused):"
echo -n "  collector runs logged: "; grep -c "run rc=" "$W/server.log"
