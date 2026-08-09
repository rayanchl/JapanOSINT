#!/bin/bash
# tests/unit/run.sh — build and run the unit tests against a scratch database.
#
# Each test #includes the .c it is testing so it can reach static functions, so
# the link line takes every object EXCEPT main.o (its own main) and the object
# of the file under test (duplicate symbols).
#
# Usage:  make -j && tests/unit/run.sh
#
# OBJDIR/CC are overridable so the same tests can run against a sanitizer
# build, which is the point of having them for a threaded change:
#   make CC="cc -fsanitize=thread" OBJ=obj-tsan BIN=bin/japanosint-tsan -j
#   OBJDIR=obj-tsan CC="cc -fsanitize=thread" tests/unit/run.sh
set -u
cd "$(dirname "$0")/../.." || exit 1     # -> native/

OBJDIR="${OBJDIR:-obj}"
CC="${CC:-cc}"
# ThreadSanitizer aborts with "unexpected memory mapping" under the ASLR
# entropy most current kernels default to; setarch -R pins it. Harmless for
# non-sanitizer runs, so it is applied whenever available.
RUNNER="${RUNNER:-}"
if [ -z "$RUNNER" ] && command -v setarch >/dev/null 2>&1; then
  setarch -R true >/dev/null 2>&1 && RUNNER="setarch -R"
fi
[ -d "$OBJDIR" ] || { echo "no $OBJDIR/ — run make first"; exit 2; }

CFLAGS="-O1 -g -Wall -Wextra -Wno-unused-parameter -pthread -Ithird_party"
CFLAGS="$CFLAGS -DJO_REPO_ROOT=\"$(cd .. && pwd)\""
LDLIBS="$(pkg-config --libs libcurl openssl 2>/dev/null) $(mecab-config --libs 2>/dev/null) -lpthread -lm -ldl -lz"

SCRATCH="${TMPDIR:-/tmp}/jo-unit-$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

fail=0
for src in tests/unit/test_*.c; do
  name=$(basename "$src" .c)
  # Which object does this test include? (duplicate symbols if also linked.)
  under=$(grep -oE '#include "\.\./\.\./[a-z_/]+\.c"' "$src" |
          head -1 | sed 's|#include "../../||; s|"$||; s|\.c$|.o|')
  objs=$(find "$OBJDIR" -name '*.o' ! -name 'main.o' \
         ${under:+! -path "$OBJDIR/$under"})

  echo "--- $name ($OBJDIR, excludes ${under:-<none>}) ---"
  # shellcheck disable=SC2086
  if ! $CC $CFLAGS "$src" $objs -o "$SCRATCH/$name" $LDLIBS 2>"$SCRATCH/$name.cc"; then
    echo "BUILD FAILED"; sed -n '1,20p' "$SCRATCH/$name.cc"; fail=1; continue
  fi
  JO_DB="$SCRATCH/$name.db" $RUNNER "$SCRATCH/$name"
  rc=$?
  [ $rc -eq 0 ] || { echo "FAILED (exit $rc)"; fail=1; }
done

exit $fail
