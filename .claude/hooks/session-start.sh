#!/bin/bash
# SessionStart hook — make `make -C native` work in Claude Code on the web.
#
# A fresh web container has no C build dependencies, so the first thing any
# session did was fail on `curl/curl.h: No such file or directory`, then on
# `mecab.h` once curl was fixed. SQLite, cJSON and mongoose are vendored in
# native/third_party/, so only the system libraries below are missing.
#
# The package list is deliberately the SAME one .github/workflows/ci.yml
# installs — if the two drift, CI and the web session stop agreeing about what
# "builds" means. Keep them in sync (docs/BUILD.md §1 is the prose copy).
set -euo pipefail

# Local runs already have a working toolchain; this is a web-container fixup.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

# Idempotent: the container state is cached after the hook completes, so on a
# resumed/cleared session the headers are already there and we skip apt.
if [ ! -e /usr/include/mecab.h ] || \
   ! find /usr/include -name curl.h -path '*curl*' -print -quit | grep -q .; then
  export DEBIAN_FRONTEND=noninteractive

  # Third-party PPAs baked into this image (deadsnakes, ondrej/php) are not
  # reachable through the egress proxy and make `apt-get update` exit non-zero
  # even when every list we actually need refreshed fine. Tolerate that here
  # rather than let `set -e` kill the hook over an unrelated repository.
  $SUDO apt-get update -qq || true

  $SUDO apt-get install -y --no-install-recommends \
    libcurl4-openssl-dev libssl-dev libmecab-dev mecab zlib1g-dev \
    mecab-ipadic-utf8
  # mecab-ipadic-utf8 is the dictionary MeCab needs at runtime. core/fts.c
  # degrades gracefully without it (g_init_failed, Latin passthrough), but then
  # the Japanese FTS path — the half most likely to break — is never exercised.
fi

# `--list-sources`, `--run` and `make selftest` all open a database and apply
# the schema; with no JO_DB they die on "[db] open failed". Point them at the
# scratch dir so a schema-applied DB is never mistaken for tracked data, the
# same split CI makes with runner.temp.
if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
  {
    echo "export JO_DB=/tmp/japanosint-session.db"
    echo "export JO_SCHEMA=${CLAUDE_PROJECT_DIR:-$PWD}/native/core/schema.sql"
  } >> "$CLAUDE_ENV_FILE"
fi
