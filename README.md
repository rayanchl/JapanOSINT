# OSINTsaas / JapanOSINT

An open-source-intelligence engine: a few thousand public data sources, fetched
on their own schedules into one searchable, geo-aware intel store, plus an
LLM-driven OSINT search that dispatches those same sources against an entity
you care about.

It is **one C binary and one iOS app**.

```
native/     the engine — a single C binary, bin/japanosint
              collectors/sources/*.c   one data source per file (1,016 files)
              core/                    scheduler, intel store, FTS, entity graph,
                                       alerts, LLM pipeline, HTTP API
              lib/                     shared parsers/clients (RSS, CSV, geo, ws…)
              third_party/             vendored SQLite, cJSON, mongoose
              tools/lint_sources.py    mechanical checks over the collector tree
ios/        the live client — SwiftUI app (map, intel, dashboard, cases, breach)
grammars/   GBNF grammars constraining the LLM's structured output
docs/       see docs/BUILD.md and docs/pipeline.md first
client/     DORMANT React web app — not served, not started. See client/README.md
scripts/    llama-server launcher, breach-corpus generator
launch.sh   the orchestrator: build → server → llama → status
```

Measured 2026-08-09: 221,534 lines of C outside `third_party/` (163,933 of it
collectors) and 39,181 lines of Swift.

## Quick start

```sh
./launch.sh up          # freeze stale procs → build → server :4000 → llama :8080
./launch.sh tags        # every env knob and flag, grouped
./launch.sh down
```

Build and test the engine on its own:

```sh
cd native
make -j                 # -> bin/japanosint
make selftest
make unit
make lint-sources
```

Prerequisites, sanitizer builds, the `JO_REPO_ROOT` story and CI are all in
[`docs/BUILD.md`](docs/BUILD.md).

## How many sources are there?

Ask the tree, never a document:

```sh
make -C native source-count           # macro-aware count of registered source_defs
native/bin/japanosint --list-sources  # what the built binary registered
```

As of 2026-08-09 that reports **2,563 registered `source_def`s** (1,104 written
directly, 1,459 produced by per-file registration macros) — a figure verified
by diffing the static count against the built binary's `--list-sources` output
id-for-id, with zero difference in either direction. The number moves with
every commit, which is exactly why it is derived rather than typed here.

That derivation matters. Many collector files define a local macro — `RSSX`,
`RSS`, `DEF` and friends — that expands to a complete `source_def` *plus* its
own `REGISTER_SOURCE`; a single `RSSX(...)` line in
`native/collectors/sources/arxiv_feeds.c` is 41 sources. A
`grep -c REGISTER_SOURCE` therefore undercounts badly, and six different wrong
source counts (150+, 286, 313, 318, 476, ~551) were committed to this repo
before anyone counted properly. `native/tools/lint_sources.py` expands the
macros and is the only counter that agrees with the running binary.

## Architecture

Every data source is one `source_def` (`native/source.h`) that self-registers
at load time. The scheduler runs the due ones; each emits records through the
single `intel_sink` chokepoint (`native/core/intel.c`) which upserts into
`intel_items`, indexes them into FTS5 (MeCab-segmented for Japanese), runs
near-duplicate detection, extracts entities into the entity graph, and
evaluates alert rules. The OSINT search pipeline reuses the *same* registry:
an LLM picks source ids, they run pivoted on an entity, and the results are
both persisted as intel and synthesised into an answer.

Full walkthrough: [`docs/pipeline.md`](docs/pipeline.md).
Writing a new source: [`native/collectors/SOURCE_AUTHORING_CONTRACT.md`](native/collectors/SOURCE_AUTHORING_CONTRACT.md).

## Clients

* **`ios/` — the live client.** SwiftUI: map with per-layer feature caching,
  intel feed, source dashboard, cases, entity graph, camera viewer, breach
  check, plus a share extension and widgets.
* **`client/` — dormant.** A React/Vite/MapLibre web app from the Node era.
  Nothing builds, serves or starts it; the C server has no static handler and
  404s every non-`/api` path. Read [`client/README.md`](client/README.md)
  before touching it.

## History, so the tree makes sense

There used to be a Node backend at `server/` (Express + SQLite + node-cron +
WebSocket). It was **deleted on 2026-05-17** and fully replaced by the C
engine. Nothing in this repo runs `npm run dev` any more. Two artefacts of that
era survive on purpose: the contract-parity fixtures in
`native/tests/contract/` (byte-level Node baselines that can never be
re-captured — treat them as read-only) and the `client/` app above.

## License

MIT
