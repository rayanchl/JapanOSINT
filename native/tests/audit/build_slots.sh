#!/usr/bin/env bash
# Rebuild every auditor build slot. Kept as a file, not an inline loop: passing
# a shell loop through `wsl.exe -- bash -c` from PowerShell eats the `$var`.
set -u
cd "$(dirname "$0")/../.." || exit 1
SLOTS="${*:-a0 a1 a2 a3 a4 a5 a6 a7 a8 a9}"
for s in $SLOTS; do
  bash tests/audit/agent_build.sh "$s" &
done
wait
echo "--- slot binaries ---"
ls -l --time-style=+%H:%M bin/japanosint-a? 2>/dev/null
