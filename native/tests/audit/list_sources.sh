#!/usr/bin/env bash
# Emit "id<TAB>collector<TAB>interval" for every registered source.
#
# --list-sources prints a fixed-width human table; this is the machine form the
# audit harness slices. Run it against a scratch JO_DB so enumerating sources
# never touches the real database (the binary seeds `sources` on open).
set -euo pipefail
NATIVE="$(cd "$(dirname "$0")/../.." && pwd)"
: "${JO_DB:=${TMPDIR:-/tmp}/jo_list.db}"
: "${JO_SCHEMA:=$NATIVE/core/schema.sql}"
export JO_DB JO_SCHEMA

"$NATIVE/bin/japanosint" --list-sources 2>/dev/null \
  | sed -E 's/^([^ ]+) +collector=([^ ]+) +interval=(-?[0-9]+)$/\1\t\2\t\3/'
