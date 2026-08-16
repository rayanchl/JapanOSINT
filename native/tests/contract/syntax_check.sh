#!/bin/bash
# tests/contract/syntax_check.sh — compile-check one collector in isolation.
#
# Lets many authors validate their own files in parallel without racing on the
# shared obj/ tree or bin/japanosint: each run compiles into its OWN throwaway
# object directory, so N of these are safe concurrently.
#
#   tests/contract/syntax_check.sh collectors/sources/my_source.c [more.c ...]
#
# Exits non-zero on the first file that does not compile warning-free.
#
# WHY -c AND -O2 RATHER THAN -fsyntax-only. -fsyntax-only stops before code
# generation, and GCC's most valuable diagnostics for this codebase are
# produced BY the optimiser: -Wformat-truncation, -Wstringop-truncation,
# -Wmaybe-uninitialized and -Wuse-after-free all go silent without it. A
# collector that silently truncates an snprintf'd URL or title is exactly the
# defect class the audit spent a session on, so the check has to be able to
# see it. The object file is thrown away; only the diagnostics matter.
set -u
cd "$(dirname "$0")/../.." || exit 1     # -> native/

REPO_ROOT="$(cd .. && pwd)"
CC="${CC:-cc}"
if ! command -v "${CC%% *}" >/dev/null 2>&1; then
  # Reported repeatedly by authors on Windows: Git-Bash has no compiler, so
  # this failed with a bare "cc: command not found" and looked like a broken
  # script rather than a missing toolchain.
  echo "error: compiler '${CC%% *}' not found." >&2
  echo "  On Windows, run this from WSL (the tree builds under the Ubuntu" >&2
  echo "  toolchain), or set CC=<compiler>." >&2
  exit 127
fi
TMPO="$(mktemp -d "${TMPDIR:-/tmp}/jo-synchk-XXXXXX")"
trap 'rm -rf "$TMPO"' EXIT

CF="-c -O2 -Wall -Wextra -Wno-unused-parameter -Ithird_party"
# Must mirror the Makefile's -iquote pair. Collectors say #include "source.h"
# and #include "lib/feedlib.h" -- depth-independent forms that only resolve
# through these paths -- so without them this script fails on essentially every
# collector with "source.h: No such file or directory" and looks like 893
# broken files instead of one stale flag list. A second copy of the compiler
# flags is a standing trap; if the Makefile's ever change, change them here too.
CF="$CF -iquote . -iquote collectors/sources"
CF="$CF -DJO_REPO_ROOT=\"$REPO_ROOT\""
CF="$CF $(pkg-config --cflags libcurl openssl 2>/dev/null) $(mecab-config --cflags 2>/dev/null)"

fail=0
for f in "$@"; do
  out=$($CC $CF "$f" -o "$TMPO/$(basename "$f" .c).o" 2>&1)
  if [ -n "$out" ]; then
    echo "=== $f ==="
    echo "$out"
    fail=1
  else
    echo "OK  $f"
  fi
done
exit $fail
