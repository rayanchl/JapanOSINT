#!/usr/bin/env bash
#
# Start the single LLM runtime for JapanOSINT: a local llama.cpp `llama-server`
# hosting the tuned model. This is the model host only — NOT the OSINTsaas C
# app. Node (llmClient.js) talks to it on :8080 for every LLM use in the
# codebase (entity NER, keyword/geocode enrichment, the ported OSINT pipeline).
#
# Launch flags mirror OSINTsaas/start-fullstack.sh verbatim so prompt/grammar
# behaviour is identical to the tuned original.
#
# Overridable via env:
#   LLAMA_BIN        path to the llama-server binary
#   OSINT_MODEL_PATH path to the .gguf model
#   LLAMA_PORT       port (default 8080)
set -euo pipefail

LLAMA_PORT="${LLAMA_PORT:-8080}"

# Self-contained: binary + model are vendored into JapanOSINT. Paths resolve
# relative to this script (JapanOSINT/scripts/) so the repo is
# relocatable and has no OSINTsaas runtime dependency.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

LLAMA_BIN="${LLAMA_BIN:-$REPO_ROOT/native/llama/llama-server}"
OSINT_MODEL_PATH="${OSINT_MODEL_PATH:-$REPO_ROOT/models/gpt-oss-20b-Q4_K_M.gguf}"

# Already up? llama.cpp exposes /health.
if curl -s "http://localhost:${LLAMA_PORT}/health" >/dev/null 2>&1; then
  echo "[start-llama] llama-server already healthy on :${LLAMA_PORT}"
  exit 0
fi

if [ -z "$LLAMA_BIN" ] || [ ! -x "$LLAMA_BIN" ]; then
  echo "[start-llama] ERROR: llama-server binary not found." >&2
  echo "  Expected vendored binary at $REPO_ROOT/native/llama/llama-server (set LLAMA_BIN to override)." >&2
  exit 1
fi
if [ ! -f "$OSINT_MODEL_PATH" ]; then
  echo "[start-llama] ERROR: model not found at $OSINT_MODEL_PATH" >&2
  echo "  Set OSINT_MODEL_PATH to the gpt-oss-20b .gguf. Search degrades gracefully without it." >&2
  exit 1
fi

# CONTEXT SIZE IS PART OF THE SEARCH PIPELINE'S CONTRACT, NOT A TUNING KNOB.
#
# It was 16384, and the OSINT search analysis prompt did not fit. The phase-1
# prompt carries the entity-pivot service catalogue so the model can route on
# what a service actually does; unbounded that catalogue was 207,353 tokens and
# llama-server answered every single search with
#
#   request (207353 tokens) exceeds the available context size (16384 tokens)
#
# core/osint_dispatch.c now bounds the catalogue to
# JO_PROMPT_SERVICE_CATALOGUE_CHARS (default 32768 characters) and states the
# bound in-band, which brings the whole prompt to roughly 18-19k tokens with
# every registered service still listed. 32768 is the smallest context that
# holds that plus the 2048-token completion with room to spare, so the shipped
# default and the shipped launcher agree by construction.
#
# If you deliberately run a smaller context, lower
# JO_PROMPT_SERVICE_CATALOGUE_CHARS to match. A mismatch no longer fails
# silently — the run is reported degraded with code `llm_http_error` and the
# server log prints the prompt's byte size next to it — but it does fail.
LLAMA_CTX="${LLAMA_CTX:-32768}"

echo "[start-llama] launching llama-server :${LLAMA_PORT} (model: $OSINT_MODEL_PATH, ctx: $LLAMA_CTX)"
exec "$LLAMA_BIN" \
  -m "$OSINT_MODEL_PATH" \
  --port "$LLAMA_PORT" \
  --host 127.0.0.1 \
  --ctx-size "$LLAMA_CTX" \
  --n-gpu-layers 35 \
  --reasoning-format auto \
  -fa auto \
  --jinja
