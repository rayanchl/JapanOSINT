#!/usr/bin/env bash
# Emit "id<TAB>collector<TAB>interval" for every registered source.
set -euo pipefail
NATIVE="$(cd "$(dirname "$0")/../.." && pwd)"
"$NATIVE/bin/japanosint" --list-sources 2>/dev/null \
  | sed -E 's/^([^ ]+) +collector=([^ ]+) +interval=([0-9-]+)$/\1\t\2\t\3/'
