# OSINTsaas / JapanOSINT

An open-source-intelligence engine: a few thousand public data sources, fetched
on their own schedules into one searchable, geo-aware intel store, plus an
LLM-driven OSINT search that dispatches those same sources against an entity
you care about.

It is **one C binary**, with two clients: a web site and a SwiftUI app.

```
native/     the engine — a single C binary, bin/japanosint
              collectors/sources/*.c   one collector family per file (1,188 files;
                                       a file may register many sources)
              core/                    scheduler, intel store, FTS, entity graph,
                                       alerts, LLM pipeline, HTTP API
              lib/                     shared parsers/clients (RSS, CSV, geo, ws…)
              third_party/             vendored SQLite, cJSON, mongoose
              tools/lint_sources.py    mechanical checks over the collector tree
client/     the web site — React/Vite app, served by its own Vite pod (:5173)
ios/        the SwiftUI app (map, intel, dashboard, cases, breach)
grammars/   GBNF grammars constraining the LLM's structured output
docs/       see docs/BUILD.md and docs/pipeline.md first
scripts/    llama-server launcher, breach-corpus generator
launch.sh   the orchestrator: build → server → web → llama → status
```

Measured 2026-08-10: ~276,000 lines of C outside `third_party/` (~208,000 of it
collectors) and ~40,000 lines of Swift. Re-derive rather than trust these:
`git ls-files '*.c' '*.h' | grep -v third_party | xargs wc -l | tail -1`.

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

Run them. The two agree id-for-id, in both directions — that equality is the
property worth checking, and it is the reason the number itself is not typed
here. (It was typed here, seven times, and every one went stale; the last one
was low by a factor of 3.5 because the counter could not see the hpengine row
tables and nobody re-ran it.)

That derivation matters. Many collector files define a local macro — `RSSX`,
`RSS`, `DEF` and friends — that expands to a complete `source_def` *plus* its
own `REGISTER_SOURCE`; a single `RSSX(...)` line in
`native/collectors/sources/arxiv_feeds.c` is 41 sources. A
`grep -c REGISTER_SOURCE` therefore undercounts badly, and six different wrong
source counts (150+, 286, 313, 318, 476, ~551) were committed to this repo
before anyone counted properly. `native/tools/lint_sources.py` expands the
macros — and, since 2026-08-10, also reads the `HP_REGISTER_TABLE` row tables in
`collectors/sources/hp*_*.c`, which had been contributing 601 invisible sources.
It is the only counter that agrees with the running binary.

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

* **`client/` — the web site.** React + Vite + MapLibre GL/deck.gl: map with
  the layer catalogue, intel feed and search, source dashboard, cases, camera
  discovery thread. `./launch.sh up` starts it as the **web pod**: the Vite
  dev server on `:5173` (`WEB_PORT`), whose `/api` proxy forwards to the
  server pod (`JO_API_TARGET`, read by `client/vite.config.js`). The C server
  itself serves only `/api` — it has no static-file handler — so the site is
  the Vite server, not a bundle the binary hands out.
  Requests carry the session: `client/src/auth/session.js` wraps
  `window.fetch` so every `/api` call sends `Authorization: Bearer <token>`
  and `X-Tenant-Id`, refreshing once on a 401. Two things cannot go through
  that wrapper and are handled separately: `EventSource` (the search progress
  stream opens with the per-run `?key=` the server hands the starter) and
  `<img>` (camera images are fetched and shown from a blob URL —
  `client/src/utils/authedImage.js`).
  There is no WebSocket: the server has no `/ws`, so the client's socket path
  is off unless built with `VITE_JO_WEBSOCKET=1`, and its hooks poll instead
  (sources every 60 s, camera run status every 4–20 s). `client/README.md`
  has the details.
* **`ios/` — the SwiftUI app.** Map with per-layer feature caching, intel
  feed, source dashboard, cases, entity graph, camera viewer, breach check.
  The Xcode project has exactly **one** target, the app (iOS and macOS). The
  share extension (`ios/JapanOsintShare/`) and the WidgetKit widgets and Live
  Activity (`ios/JapanOsintWidgets/`) are source files that **no target
  compiles**, so they do not ship, and the App Group they would share data
  through is not configured (the app gates that code off at runtime).
  `ios/EXTENSION_TARGETS_TODO.md` is the procedure for adding the two
  targets.

## History, so the tree makes sense

There used to be a Node backend at `server/` (Express + SQLite + node-cron +
WebSocket). It was **deleted on 2026-05-17** and fully replaced by the C
engine. Two artefacts of that era survive on purpose: the contract-parity
fixtures in `native/tests/contract/` (byte-level Node baselines that can never
be re-captured — treat them as read-only), and the web client in `client/`,
which now runs against the C server (see Clients above). This README used to
say that `client/` had been removed on 2026-08-10 and sent no `Authorization`
header; neither was true of this tree — the directory is here, `launch.sh`
starts it as the site, and `client/src/auth/session.js` sends the bearer.

## License

MIT
