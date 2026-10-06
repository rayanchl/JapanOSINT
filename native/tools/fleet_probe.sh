#!/usr/bin/env bash
# Fleet probe: run every scheduled source once against its real upstream and
# record what came back. Produces a TSV of id, rc, record count and duration.
#
# Why this exists: the tree has six different tracked source counts and a
# "sources with data" number nobody trusts. The only authority on whether a
# collector works is the collector running. This is that, fleet-wide.
#
# Each RUN gets its own SQLite file: a copy of the warm database the source
# listing below just created (schema applied, migrations run, sources seeded),
# deleted afterwards. It used to be `w$(( $$ % 64 )).db` — a slot derived from
# the worker's PID, so two concurrent workers whose PIDs differed by a multiple
# of 64 wrote the same file and each one's counts included the other's rows.
# The scheduler's own per-run summary line
# ("[sched] <id> run rc=<rc> records=<n> <ms>ms") is the parse target.
#
# Portable to macOS: no shuf, `date +%3N`, timeout(1), `xargs -a/-d` or GNU
# `\?` in sed — each has a fallback or a portable spelling below.
set -u

BIN="${BIN:-$HOME/jobuild/jo}"
OUT="${OUT:-$HOME/jotest/fleet}"
JOBS="${JOBS:-10}"
TIMEOUT="${TIMEOUT:-45}"

mkdir -p "$OUT/logs" "$OUT/db"

shuffle() {
  if command -v shuf >/dev/null 2>&1; then shuf
  else python3 -c 'import random,sys; l=sys.stdin.read().splitlines(); random.shuffle(l); print("\n".join(l))'
  fi
}
now_ms() {
  if [ -n "${EPOCHREALTIME:-}" ]; then
    local t=${EPOCHREALTIME/[.,]/}; echo $(( t / 1000 ))
  else python3 -c 'import time; print(int(time.time()*1000))'
  fi
}
# timeout(1) is GNU coreutils. gtimeout is the Homebrew name; failing both, a
# perl alarm. The alarm kills with SIGALRM (exit 142), so 142 is a timeout too.
run_capped() {
  local secs="$1"; shift
  if command -v timeout >/dev/null 2>&1; then timeout "$secs" "$@"
  elif command -v gtimeout >/dev/null 2>&1; then gtimeout "$secs" "$@"
  else perl -e 'alarm shift; exec @ARGV or exit 127' "$secs" "$@"
  fi
}

# Source list. Shuffled deliberately: 613 of these point at news.google.com and
# ~142 at reddit, so an alphabetical order would fire each cohort as one burst
# and measure the upstream's rate limiter rather than the collector.
if [ ! -s "$OUT/ids.txt" ] || [ ! -s "$OUT/db/list.db" ]; then
  rm -f "$OUT/db/list.db" "$OUT/db/list.db-wal" "$OUT/db/list.db-shm"
  JO_DB="$OUT/db/list.db" "$BIN" --list-sources 2>/dev/null \
    | awk '{print $1"\t"$2"\t"$3}' \
    | sed 's/collector=//; s/interval=//' > "$OUT/all.tsv"
  awk -F'\t' '$3 > 0 {print $1}' "$OUT/all.tsv" | shuffle > "$OUT/ids.txt"
  awk -F'\t' '$3 == 0 {print $1}' "$OUT/all.tsv" > "$OUT/pivot_ids.txt"
fi

echo "scheduled sources: $(wc -l < "$OUT/ids.txt")"
echo "entity-pivot sources (skipped, need an entity arg): $(wc -l < "$OUT/pivot_ids.txt")"

probe_one() {
  id="$1"
  # A unique file per run, seeded from the warm listing DB. sqlite3's .backup
  # would be cleaner, but a plain copy of a checkpointed file is enough here
  # and needs nothing beyond cp.
  db=$(mktemp "$OUT/db/run.XXXXXX") || return 1
  cp "$OUT/db/list.db" "$db"
  log="$OUT/logs/${id}.log"
  start=$(now_ms)
  JO_DB="$db" run_capped "$TIMEOUT" "$BIN" --run "$id" >"$log" 2>&1
  ec=$?
  end=$(now_ms)
  wall=$(( end - start ))
  rm -f "$db" "$db-wal" "$db-shm"

  # The scheduler prints one summary line per run; trust it over the exit code,
  # which main.c collapses to 0/1/2 and which timeout(1) overwrites with 124.
  line=$(grep -m1 '^\[sched\] .* run rc=' "$log" || true)
  if [ -n "$line" ]; then
    rc=$(sed -nE 's/.* rc=(-?[0-9]*) .*/\1/p' <<<"$line")
    rec=$(sed -nE 's/.*records=(-?[0-9]*) .*/\1/p' <<<"$line")
  else
    rc=""; rec=""
  fi
  [ -z "$rc" ] && rc="NA"
  [ -z "$rec" ] && rec="NA"

  if [ "$ec" = "124" ] || [ "$ec" = "142" ]; then verdict="TIMEOUT"
  elif grep -q '^unknown source' "$log"; then verdict="UNKNOWN_ID"
  elif [ "$rc" = "NA" ]; then verdict="NO_SUMMARY"
  elif [ "$rc" = "0" ] && [ "$rec" != "0" ]; then verdict="OK"
  elif [ "$rc" = "0" ] && [ "$rec" = "0" ]; then verdict="EMPTY_OK"
  elif [ "$rec" != "0" ] && [ "$rec" != "NA" ]; then verdict="ROWS_BUT_ERR"
  else verdict="FAIL"
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$id" "$verdict" "$rc" "$rec" "$wall" "$ec"
}
# The listing DB is the per-run template; fold its WAL in so a copy of the
# main file alone is complete.
sqlite3 "$OUT/db/list.db" 'PRAGMA wal_checkpoint(TRUNCATE);' >/dev/null 2>&1 || \
  python3 -c 'import sqlite3,sys; sqlite3.connect(sys.argv[1]).execute("PRAGMA wal_checkpoint(TRUNCATE)")' "$OUT/db/list.db"

export -f probe_one now_ms run_capped
export BIN OUT TIMEOUT

printf 'id\tverdict\trc\trecords\twall_ms\texit\n' > "$OUT/results.tsv"
tr '\n' '\0' < "$OUT/ids.txt" | \
  xargs -0 -P "$JOBS" -I{} bash -c 'probe_one "$@"' _ {} \
  >> "$OUT/results.tsv" 2>"$OUT/probe.err"

echo "=== verdict summary ==="
tail -n +2 "$OUT/results.tsv" | cut -f2 | sort | uniq -c | sort -rn
echo "=== total rows collected ==="
tail -n +2 "$OUT/results.tsv" | awk -F'\t' '$4 ~ /^[0-9]+$/ {s+=$4} END {print s+0}'
echo "results: $OUT/results.tsv"
