# Backend Structure & Pipeline Report — JapanOSINT native C engine

_Generated 2026-07-20 · branch `feat/osint-batch12-c-port` · scope: `native/` (~114k LOC C)_

This report maps the backend's structure and pipelines, and inventories what can be
**unified**, what is **dead/useless**, and which **parallel pipelines** can be
consolidated. It is analysis only — no source was modified. Findings are ranked and
carry `file:line` references so each item is directly actionable.

---

## 0. Executive summary

The backend is a single native C binary (`bin/japanosint`), a completed port of a now
-deleted Node.js server. It is **architecturally clean and genuinely well-factored**:

- **True dead code is negligible** — the Makefile globs every `.c`, so nothing is
  orphaned by the build; tree-wide there are only **2 dead static functions**, **1
  dead include file**, and a handful of **dead grammar/schema entries**.
- **The real debt is duplication**, almost all of it copy-pasted micro-helpers and
  boilerplate across the **630 collector files** — added in rapid batches (the last four
  commits alone add `+111`, `+84`, `+66`, `+34` collectors). This is where "unify what
  can be unified" pays off: **~15,000 LOC is mechanically removable** with near-zero
  behavioral risk.
- There is **one unifying abstraction already in place** — every long-running flow is a
  `source_def` with a `run(ctx, sink)` function, driven by one scheduler and emitting
  through one `intel_sink`. Most "multiple pipelines" concerns resolve to *fragmented
  instances of this one pattern* that should be re-consolidated.

**Highest-value actions:** two shared collector helpers (`probe_emit_status`,
`lib/restcollect`) remove ~7,000 LOC; a `lib/`-hosted helper set (timestamps, cJSON
accessors, url-encode, extract_json) removes ~650 LOC across ~90 files; a `core/apiutil`
removes ~300 LOC of byte-parity-critical formatters; deleting a triplicated ADS-B
collector removes 567 LOC **and** fixes a live bug.

---

## 1. System structure

```
native/
  main.c            entry point; modes: (default) selftest · --serve · --sched
                    · --run <id> [entity] · --list-sources · --wakati · --ingest
  registry.c        runtime source registry (flat array, constructor-registered)
  source.h          THE core ABI: intel_item · intel_sink · source_ctx · source_def
  core/    (81 files, ~15.5k LOC)   HTTP server, REST APIs, pipelines, stores, LLM
  lib/     (16 modules)             shared collector helpers (feedlib, overpass, …)
  collectors/sources/ (630 .c, ~94k LOC)   one file per data source / OSINT service
  third_party/      cJSON · sqlite3 (FTS5/RTREE/JSON1) · mongoose   (vendored)
  grammars/ models/ (external, via JO_REPO_ROOT)   GBNF grammars + gguf model
```

**The one ABI (`source.h`).** There is deliberately no distinction between a "map feed"
and an "OSINT service" — both are a `source_def` (`source.h:56-76`):

```c
typedef struct {
  const char *id, *collector /* group */, *category, *type, *url, *description, *layer;
  int update_interval_sec;        /* >0 = cron-scheduled · 0 = on-demand (OSINT pivot) */
  int (*run)(const source_ctx*, intel_sink*);
  ...
} source_def;
```

Collectors self-register via a GCC-constructor macro `REGISTER_SOURCE(x)`
(`source.h:84-86`) into a flat `g_srcs[4096]` array (`registry.c`). Everything a run
produces flows through the single `intel_sink.emit()` chokepoint (`core/intel.c`) — one
upsert + FTS mirror + entity/alert path.

---

## 2. HTTP / API layer  (`core/httpd.c` + `core/*api.c`)

**Request pipeline.** Single-threaded **mongoose** event loop. `main --serve` →
`httpd_serve()` (`httpd.c:904`) → one callback `fn()` (`httpd.c:258`) handles every
event. Routing is a **linear `if`-chain of string matches** (`eq()`/`starts()`/`seg()`),
not a dispatch table — ordered specific-before-catchall. Auth gate: three pre-auth
routes, then everything under `/api/` passes `auth_check()` (`auth.c:228`, JWKS
RS256/ES256 → HS256 fallback); secondary `opgate_check` (operator triad) and
`tenant_resolve` (X-Tenant-Id) gates layer inside the chain. Handlers return a malloc'd
JSON string emitted by `reply_json()`. Off-loop work (LLM suggest, collector triggers)
runs on detached threads and replies via `mg_wakeup`; SSE search streams tracked in a
64-slot table. Sub-trees (`transitapi`, `alertsapi`, `keysapi`, `members`, `maintenance`)
delegate to a second string-dispatcher inside the module.

**API surface** — ~60 routes across: search (`/api/search/*`), sources/layers/status,
intel (`/api/intel/*`), admin+db (opgated), geo proxy, alerts, tenant-scoped
(members/keys/me/audit), entities, data layers, and transit (12 sub-routes). Every
declared handler is reachable from a route — **no un-wired handlers**.

### Unification opportunities (ranked)
| # | Pattern | Copies | Fix |
|---|---|---|---|
| A1 | `iso_now()` ISO-8601 (in API files) | ~8 in core | shared `core/apiutil` (see §7 tree-wide: 26 total) |
| A2 | sqlite `col_text()`/`ctext()` accessor | 7 | `jo_col_text(stmt,i)` |
| A3 | cJSON `add_str_or_null`/`add_num_or_null` | 6 | `jo_add_col_{text,int,real}` |
| A4 | error envelope `err`/`err_json`/`jerr`/`merr` | 5 divergent | one `jo_err(status,code,msg)` |
| A5 | `httpd.c` tenant-resolve preamble | 4 blocks | `resolve_tenant_or_reply()` |
| A6 | `httpd.c` opgate triad | 4 blocks | `opgate_or_reply()` |
| A7 | `httpd.c` body-copy idiom | 11 sites | `dup_body(hm)` |
| A8 | `uuid4`, `b64url*` | 2 + 3 | shared `jo_uuid4`, `jo_b64url*` |

**Net:** a new `core/apiutil.{c,h}` + local httpd helpers remove **~300–380 LOC** and,
more importantly, collapse **byte-parity-critical formatters** that today must be kept
hand-identical across 8+ files.

### Dead / stale surface
- `main.c:4` advertises `--selftest`, but the flag is never parsed (actual toggle is
  `--serve`; selftest is the default). Stale doc.
- `--wakati` tokenizer parity harness (`main.c:63-75`) — migration-era; dead now.
- `/api/_sse_probe` diagnostic route + `sse_probe_start/poll` (`httpd.c:70-96,280`) —
  dev smoke test compiled into the production listener.
- `POST /api/admin/restart` — no-op stub (`httpd.c:541`).
- `miscapi_follow_recent` (`httpd.c:521`) — deliberate empty-envelope stub.
- transit `/gtfs/hydrate/:orgId`, `/station-boundaries` — structurally present, always
  return empty. Consider `501 Not Implemented` so clients can tell "empty" from
  "unsupported".

---

## 3. Collector subsystem  (`collectors/sources/` — 630 files)

**Framework.** Each collector is a `source_def` (§1). `scheduler_run_source()`
(`scheduler.c:27`) builds a fresh `http_client`+`llm_client`, wraps the sink in a
`count_sink`, runs `d->run()`, then writes `fetch_log` + `anomaly_detect`.
`scheduler_loop()` crons every source with `interval>0` (staggered), skipping
`search_only`/quarantined; on-demand OSINT services (`interval<=0`) are driven by the
search pipeline instead.

**Two parallel registries (drift hazard).** (1) The **runtime** registry, built by
`REGISTER_SOURCE` constructors — what the scheduler/dispatcher actually use. (2)
`core/source_registry.gen.c` — a **frozen, metadata-only** table (420 rows) for map-layer
metadata, "GENERATED from `server/src/utils/sourceRegistry.js`" — **a generator that
lives in the deleted Node server**. New collectors don't update it; it already disagrees
with reality (see ADS-B below). Sources not in the table carry inline `source_def`
metadata via `src_meta_get()`.

**Counts:** 630 `.c` files · ~624 with `REGISTER_SOURCE` · 420 map-metadata rows. The 7
files with no `REGISTER_SOURCE` (`breach_checker`, `email_reputation`, `email_validator`,
`maigret`, `social_intel`, `social_platforms`, `username_osint`) are **not dead** — they
export `jo_*_run()` fused by aggregator collectors (`social_fuse.h`).

### 3a. Duplicate / overlapping collectors
- **ADS-B flight tracking — implemented 3×, with a live bug.**
  `flight_adsb.c` (829 LOC, id `flight-adsb`, superset, is the map layer) and
  `plane_adsb.c` (567 LOC, id `plane-adsb`) are near line-for-line copies hitting the
  **same OpenSky Japan bbox every 60s**. `unified_flights.c` re-invokes `plane-adsb`,
  but `.gen.c:309` claims it merges `flight-adsb`. Result: three collectors triplicate
  OpenSky load and the richer collector is disconnected from the fused output.
  **→ Keep `flight_adsb.c`; repoint `unified_capture(…,"plane-adsb")`→`"flight-adsb"`;
  delete `plane_adsb.c` (−567 LOC); fix `.gen.c:309`.**
- **`*_world.c` "N2" splits** — `academic_world.c`+`academic2_world.c`,
  `crypto_world.c`+`crypto2_world.c` are *different* sources in one domain, split only
  because a second batch arrived. Merge back to one file per domain.
- **Breach probe stubs** — `hibp_breach.c`, `dehashed_breach.c`, `leakcheck_breach.c`,
  `snusbase_breach.c`, `breach_directory.c` are 54–56 LOC each, all just
  `probe_head()` → emit a reachability status (no real data). Collapse to one
  status-table row or delete (−~280 LOC). Keep the real `breach_checker.c` +
  `breach_index_svc.c`.
- **People / WHOIS families** — `people_finder/research/world.c`,
  `whois_lookup/reverse_whois/whoisxml_reverse.c` overlap; consolidate after a
  functional review.

### 3b. Shared-pattern unification (biggest absolute win)
- **REST fetch→parse→emit block** repeated in **~160 files** (~40 LOC each). Propose
  `lib/restcollect.h`: `collector_fetch_json(ctx,url,hdrs,&json)` + table-driven
  `emit_from_json(sink,json,field_map)`. **≈ −4,000 LOC.**
- **Portal-status probe stubs** — **88 files** repeat a ~35-LOC tag/property/emit block
  over `lib/probe.h`. Propose `probe_emit_status(sink,id,url,key_env,title,…)`; each stub
  drops to ~8–10 lines. **≈ −3,000 LOC.**
- **Registration macros** — `AC2_DEF`, `C2_DEF`, and per-file variants re-declare the
  same 5-line macro. One shared `REGISTER_OSINT_SOURCE(...)` header removes dozens.

---

## 4. Pipelines

Eight distinct flows. **(b), (i) are sub-stages, not independent pipelines.**

| # | Pipeline | Entry | Writes |
|---|---|---|---|
| a | **OSINT search** — analyze→assign→dispatch→followup rounds→synthesis→entity graph | `pipeline.c:169` (via `searchapi_analyze` detached thread) | `progress` (SSE), `intel_items`, `entities*` |
| b | OSINT dispatch (sub of a) | `osint_dispatch.c:183` (dual_sink) | `intel_items` |
| c | **Collector scheduler** — cron per `interval` | `scheduler.c:131`→`:95` | `intel_items`, `fetch_log`, `collector_anomaly` |
| d | **Intel sink** — the one upsert+FTS path | `intel.c:147` `emit` | `intel_items`, FTS |
| e | **Breach ingest** — offline sharded index | `breach_index.c:219` (`--ingest`) | `$JO_BREACH_DIR` shards |
| f | **Maintenance pod** — detect→triage→repair | 3 units (see below) | `collector_anomaly`, `collector_repair`, `sources.quarantined`, `collector_url_overrides` |
| g | **Entity-enrichment pod** — extract→resolve | `entity_enrich.c:237` (via `llm_enricher.c`) | `entities`, `entity_mentions/relationships/merges` |
| h | **Transit/station clustering** | `transport_cluster_runner.c:64` | `station_clusters`, `station_line_dots`, `station_footprints` |
| i | FTS indexing (sub-stage of d/entitystore) | `fts.c:61` | FTS |

### "Multiple pipelines" to unify
- **U1 — the maintenance pod is ONE pipeline fragmented into three** (highest payoff).
  Detect (`maint_detect.c:92`, inline in scheduler), triage (`anomaly_triage.c`, pseudo
  -source on 60s cron), repair (`collector_repair.c`, 120s cron). They duplicate re-fetch,
  source-bundle building, `records_baseline` (2 copies: `maint_detect.c:44`,
  `collector_repair.c:134`), `is_quarantined` (3 copies: `scheduler.c:67`,
  `collector_repair.c:170`, + one more), `extract_json`, env helpers, and cJSON helpers.
  **→ Extract `core/maint.c` (shared refetch/bundle/baseline); keep the two pseudo-sources
  as thin `run()` shims.**
- **U2 — collapse 4 `llm_complete`+`extract_json`+field-pull reimplementations** into one
  `llm_complete_json()` (natural home `core/llm.c`), deleting the 4 `extract_json` copies
  (`pipeline.c:17`, `entity_enrich.c:20`, `anomaly_triage.c:31`, `collector_repair.c:53`).
- **U3 — one instrumented intel-sink decorator.** `count_sink` (`scheduler.c:19`) and
  `dual_sink` (`osint_dispatch.c:139`) are two hand-rolled decorators over the same sink
  (dual = instrumented + capture flag).
- **U4 — move maintenance-pod prompts into `prompts.c`.** Three inline `SYS*` literals
  (`anomaly_triage.c:122`, `collector_repair.c:306,320`) bypass the otherwise-centralized
  `prompts.c`.
- **Do NOT unify** the two LLM calling conventions (chat+schema vs raw GBNF) or the
  progress-vs-fetch_log split — both are deliberate, load-bearing design choices with
  rationale in existing comments (`llm.h:34`, `pipeline.c:452`, `progress.h:16`).

### LLM / grammar dead entries
`schema_load` is only ever called for `osint_analysis`; 6 of 7 `.schema.json` slots are
never loaded. `page_analysis` is **dead in both loaders** (drop it from `GRAMMAR_NAMES`
`prompts.c:604` + delete files). `osint_analysis.gbnf` is dead (superseded by the dynamic
schema). **Portability gap:** there is no `native/grammars/`; both loaders resolve against
`JO_REPO_ROOT` (default `/Users/rayan/JapanOSINT`), so on any other checkout every
constrained-generation call silently falls back to ungrammared output — vendor the files
into `native/grammars/` or log a one-time warning.

---

## 5. lib / build / artifacts

- **Build (`Makefile`):** wildcard-globs `core/*.c`, `lib/*.c`, `collectors/sources/*.c`
  → single binary. **No orphaned `.c`.** Caveat: the unconditional glob means one broken
  collector fails the whole build.
- **`lib/` (16 modules):** all have live callers — **no dead lib module**. The four
  single-caller modules (`bigfile`, `bloom`, `sgp4`, `zipread`) are niche but wired in.
- **Committed build artifacts (should be untracked/gitignored):**
  - `native/bin/japanosint` — **tracked** (3.7 MB in HEAD / 19.6 MB working). `.gitignore`
    covers `native/obj/` but **not `native/bin/`**.
  - `native/.run/` (pid/log runtime state) — add to `.gitignore`.
  - `native/llama/*.dylib` (macOS) shipped alongside `*.so` (Linux) — off-platform dead
    weight (tens of MB).
  - Stale `.gitignore` entry `server/data/` (the `server/` tree is deleted).
- **Node remnants:** none shipping — the Node backend is fully removed (commit `4db45c8`);
  "Node" survives only in comments. Stale port-handoff docs: `native/OSINT_ENGINE_STATUS.md`,
  `native/P5_REMAINING.md`, `native/P6_SWEEP_STATUS.md`. Broken parity harness
  `native/tests/contract/run.sh` (hardcoded dead macOS path + diffs the removed Node
  server) + its `*.node.json` fixtures.

---

## 6. Dead code & markers (tree-wide)

Minimal — this is a well-maintained port:
- **Dead static functions: 2** — `bo_world.c:35 bo_attr`, `flight_adsb.c:212 arr_num`.
- **Dead exported functions: 0.**
- **Dead file: 1** — `collectors/sources/_person_links.inc` (0 includes; `people_finder.c:105`
  states it is "left unused").
- **Markers:** 0 FIXME/HACK/XXX/DEPRECATED in source; 3 descriptive TODOs (upstream-API
  notes, not debt); "legacy"/"unused" hits are all legitimate.

---

## 7. Cross-cutting duplication (tree-wide helper copies)

The dominant debt. A shared-home pattern already exists (`_jp_osint.inc`, included by 135
files) — the duplicating files simply don't use it.

| Helper | Copies | ~LOC | Proposed home |
|---|---|---|---|
| ISO-8601 timestamp (`iso_now`/`now_iso`/`iso_utc`/`iso_ago`) | **26** | ~190 | `lib/timefmt.{c,h}` |
| cJSON string/num accessor (`sv`/`jstr`/`num_of`/`str_of`) | **~49** | ~185 | promote `jo_sv`/`jo_nv` → `lib/cjson_get.h` |
| `url_encode`/`url_encode_dup` (byte-identical) | **13** | ~180 | `lib/urlutil.{c,h}` |
| `extract_json` (first balanced `{}`) | **4** | ~50 | `lib/llmjson.{c,h}` (with U2) |
| SHA-hex family (`sha1_20` dup + 3 singletons) | 2–5 | ~40 | `lib/hashhex.{c,h}` |
| `env_int`/`env_double`/`env_truthy` | 5 | ~30 | `core/envutil.h` |

**≈ −660 LOC** of pure duplication, ~85% from priorities 1–3, near-zero behavioral risk.
Already well-factored (leave alone): `http_client` ABI, `lower_dup`, `strset`/`tasklist`,
base64.

---

## 8. Consolidated action plan (ranked by payoff ÷ risk)

**Tier 1 — high payoff, mechanical, low risk (~11,000 LOC)**
1. `probe_emit_status()` helper → refactor 88 probe stubs. **−~3,000 LOC.**
2. `lib/restcollect.h` (`fetch_json` + field-map) → migrate ~160 REST collectors. **−~4,000 LOC.**
3. `lib/timefmt` + `lib/cjson_get` + `lib/urlutil` → delete ~88 helper copies. **−~550 LOC.**
4. Delete `plane_adsb.c`; repoint `unified_capture`→`flight-adsb`; fix `.gen.c:309`.
   **−567 LOC + fixes a live bug.**

**Tier 2 — structural unification (moderate effort)**
5. `core/apiutil.{c,h}` (§2 A1–A8) + httpd local helpers. **−~350 LOC.**
6. `core/maint.c` — consolidate the 3-way maintenance pod (U1).
7. `llm_complete_json()` + `lib/llmjson` — delete 4 `extract_json` copies (U2).
8. One instrumented sink decorator (U3); move maint prompts to `prompts.c` (U4).
9. Merge `*2_world.c` splits; collapse 5 breach probe stubs; one `REGISTER_OSINT_SOURCE` macro.

**Tier 3 — hygiene & correctness**
10. Untrack + gitignore `native/bin/`, `native/.run/`; prune off-platform `*.dylib`; drop
    stale `.gitignore` `server/data/`.
11. Delete stale docs (`OSINT_ENGINE_STATUS.md`, `P5_REMAINING.md`, `P6_SWEEP_STATUS.md`)
    and the broken `tests/contract/` Node half.
12. Remove dead symbols (`bo_attr`, `arr_num`, `_person_links.inc`) and dead grammar
    entries (`page_analysis`, `osint_analysis.gbnf`).
13. Vendor `grammars/`+`schema` into `native/` (or warn) so constrained generation isn't
    silently disabled off-machine.
14. Decide fate of `source_registry.gen.c` — re-home the generator into `native/scripts/`
    or migrate its 420 rows to inline metadata (the dual system drifts).

**Estimated total: ~15,000 LOC and ~8–10 files removed/merged, no loss of real capability.**
