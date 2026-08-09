#!/usr/bin/env bash
# Block until both sweeps have exited, then print the final verdict mix.
# The pgrep pattern is bracketed so this waiter never matches itself.
set -u
cd "$(dirname "$0")/../.." || exit 1
while pgrep -f 'audit_run[.]py --ids' >/dev/null; do sleep 20; done
python3 tests/audit/status.py
