#!/usr/bin/env bash
# Sweep with the harness timeout matched to lib/overpass.c's budget.
#
# The default 120 s harness timeout is now SHORTER than the Overpass total
# budget (300 s), so every Overpass-backed source is killed mid-flight and reads
# TIMEOUT whether or not it works. 330 s gives them their full budget plus
# margin, which is what the production scheduler allows.
set -u
cd "$(dirname "$0")/../.." || exit 1
start() { # label plan out log jobs timeout
  if pgrep -f "audit_run.py --ids tests/audit/$2" >/dev/null; then
    echo "$1: already running"; return
  fi
  nohup bash tests/audit/sweep.sh "tests/audit/$2" "tests/audit/$3" \
        "tests/audit/$4" "$5" "$6" >/dev/null 2>&1 &
  echo "$1: started pid $!"
}
start scheduled plan_scheduled.tsv results_scheduled.jsonl sweep_scheduled.log 10 330
start ondemand  plan_ondemand.tsv  results_ondemand.jsonl  sweep_ondemand.log   6 330
sleep 3
pgrep -af audit_run.py
