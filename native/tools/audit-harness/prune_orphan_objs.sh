#!/usr/bin/env bash
# Delete object files whose .c no longer exists, then relink.
#
# Removing a collector .c does NOT by itself drop the source: the stale .o keeps
# its REGISTER_SOURCE constructor in the link, and make has no rule that fires
# when a prerequisite *disappears*, so the binary is never even relinked. Both
# have to be forced.
set -u
cd "$(dirname "$0")/../.." || exit 1
OBJ="${1:-obj}"
n=0
while IFS= read -r o; do
  rel="${o#"$OBJ"/}"; src="${rel%.o}.c"
  if [ ! -f "$src" ]; then
    rm -f "$o" "${o%.o}.d"
    echo "  orphan: $src"
    n=$((n+1))
  fi
done < <(find "$OBJ" -name '*.o' 2>/dev/null)
echo "pruned $n orphaned objects from $OBJ"
