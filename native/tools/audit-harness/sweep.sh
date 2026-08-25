#!/usr/bin/env bash
# Run one sweep pass. Usage: sweep.sh <plan.tsv> <results.jsonl> <log> [jobs] [timeout]
set -u
cd "$(dirname "$0")/../.." || exit 1
PLAN="$1"; OUT="$2"; LOG="$3"; JOBS="${4:-10}"; TMO="${5:-120}"
exec python3 tests/audit/audit_run.py --ids "$PLAN" --out "$OUT" \
     --jobs "$JOBS" --timeout "$TMO" --resume 2>>"$LOG"
