#!/usr/bin/env bash
# Probe candidate endpoints for the six broken scrapers: status, type, size and
# a content fingerprint, so a real data path can be picked instead of guessing.
set -u
UA='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126 Safari/537.36'
probe() {
  printf '\n--- %s\n' "$1"
  code=$(curl -s -o /tmp/pl.body -w '%{http_code} %{content_type} %{size_download}' \
         -A "$UA" -L --max-time 25 "$1" 2>/dev/null)
  echo "    $code"
  head -c "${2:-260}" /tmp/pl.body | tr -d '\r' | tr '\n' ' '
  echo
}
for u in "$@"; do probe "$u"; done
