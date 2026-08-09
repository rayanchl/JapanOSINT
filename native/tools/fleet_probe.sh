#!/usr/bin/env bash
# Fleet probe: run every scheduled source once against its real upstream and
# record what came back. Produces a TSV of id, rc, record count and duration.
#
# Why this exists: the tree has six different tracked source counts and a
# "sources with data" number nobody trusts. The only authority on whether a
# collector works is the collector running. This is that, fleet-wide.
#
# Each worker gets its own SQLite file. The scheduler's own per-run summary
# line ("[sched] <id> run rc=<rc> records=<n> <ms>ms") is the parse target.
set -u

BIN="${BIN:-$HOME/jobuild/jo}"
OUT="${OUT:-$HOME/jotest/fleet}"
JOBS="${JOBS:-10}"
TIMEOUT="${TIMEOUT:-45}"

mkdir -p "$OUT/logs" "$OUT/db"

# Source list. Shuffled deliberately: 613 of these point at news.google.com and
# ~142 at reddit, so an alphabetical order would fire each cohort as one burst
# and measure the upstream's rate limiter rather than the collector.
if [ ! -s "$OUT/ids.txt" ]; then
  JO_DB="$OUT/db/list.db" "$BIN" --list-sources 2>/dev/null \
    | awk '{print $1"\t"$2"\t"$3}' \
    | sed 's/collector=//; s/interval=//' > "$OUT/all.tsv"
  awk -F'\t' '$3 > 0 {print $1}' "$OUT/all.tsv" | shuf > "$OUT/ids.txt"
  awk -F'\t' '$3 == 0 {print $1}' "$OUT/all.tsv" > "$OUT/pivot_ids.txt"
fi

echo "scheduled sources: $(wc -l < "$OUT/ids.txt")"
echo "entity-pivot sources (skipped, need an entity arg): $(wc -l < "$OUT/pivot_ids.txt")"

probe_one() {
  id="$1"
  slot=$(( $$ % 64 ))
  db="$OUT/db/w${slot}.db"
  log="$OUT/logs/${id}.log"
  start=$(date +%s%3N)
  JO_DB="$db" timeout "$TIMEOUT" "$BIN" --run "$id" >"$log" 2>&1
  ec=$?
  end=$(date +%s%3N)
  wall=$(( end - start ))

  # The scheduler prints one summary line per run; trust it over the exit code,
  # which main.c collapses to 0/1/2 and which timeout(1) overwrites with 124.
  line=$(grep -m1 '^\[sched\] .* run rc=' "$log" || true)
  if [ -n "$line" ]; then
    rc=$(sed -n 's/.* rc=\(-\?[0-9]*\) .*/\1/p' <<<"$line")
    rec=$(sed -n 's/.*records=\([0-9]*\) .*/\1/p' <<<"$line")
  else
    rc=""; rec=""
  fi
  [ -z "$rc" ] && rc="NA"
  [ -z "$rec" ] && rec="NA"

  if [ "$ec" = "124" ]; then verdict="TIMEOUT"
  elif grep -q '^unknown source' "$log"; then verdict="UNKNOWN_ID"
  elif [ "$rc" = "NA" ]; then verdict="NO_SUMMARY"
  elif [ "$rc" = "0" ] && [ "$rec" != "0" ]; then verdict="OK"
  elif [ "$rc" = "0" ] && [ "$rec" = "0" ]; then verdict="EMPTY_OK"
  elif [ "$rec" != "0" ] && [ "$rec" != "NA" ]; then verdict="ROWS_BUT_ERR"
  else verdict="FAIL"
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$id" "$verdict" "$rc" "$rec" "$wall" "$ec"
}
export -f probe_one
export BIN OUT TIMEOUT

printf 'id\tverdict\trc\trecords\twall_ms\texit\n' > "$OUT/results.tsv"
xargs -a "$OUT/ids.txt" -d '\n' -P "$JOBS" -I{} bash -c 'probe_one "$@"' _ {} \
  >> "$OUT/results.tsv" 2>"$OUT/probe.err"

echo "=== verdict summary ==="
tail -n +2 "$OUT/results.tsv" | cut -f2 | sort | uniq -c | sort -rn
echo "=== total rows collected ==="
tail -n +2 "$OUT/results.tsv" | awk -F'\t' '$4 ~ /^[0-9]+$/ {s+=$4} END {print s+0}'
echo "results: $OUT/results.tsv"
