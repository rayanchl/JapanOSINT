#!/usr/bin/env bash
# Does the tree still build? Prints PASS/FAIL plus any hard errors.
cd "$(dirname "$0")/../.." || exit 1
LOG=tests/audit/build_check.log
make -j8 >"$LOG" 2>&1
rc=$?
echo "make exit=$rc"
grep -E 'error:|undefined reference|No rule to make' "$LOG" | head -30
if [ "$rc" -eq 0 ] && [ -x bin/japanosint ]; then
  echo "BUILD PASS -> bin/japanosint linked $(date -r bin/japanosint '+%H:%M:%S')"
else
  echo "BUILD FAIL"
fi
