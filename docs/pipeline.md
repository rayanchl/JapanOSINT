# Pipelines

There are two, and they share one chokepoint.

1. **The collection pipeline** — the scheduler runs registered sources on their
   own intervals and everything they produce lands in `intel_items`.
2. **The OSINT search pipeline** — an LLM turns a user's query into a plan,
   dispatches a subset of the *same* sources against a pivot entity, and
   synthesises an answer.

Both write through `intel_sink` (`native/core/intel.c`). There is no second
write path. If a row is in the product, it went through there.

> Until 2026-08-09 this filename held the breach-check design plan and said
> nothing about either pipeline; that document now lives at
> [`breach-check-pipeline.md`](./breach-check-pipeline.md), which is the name
> the C sources had been citing all along.

---

## 0. What a source is

One data source is one `source_def` (`native/source.h`) in one `.c` file under
`native/collectors/sources/`, self-registering through a constructor:

```c
static const source_def my_src = {
  .id = "jma-earthquake", .collector = "environment",
  .name = "JMA Earthquake API", .update_interval_sec = 60,
  .run = run_jma_earthquake, .layer = "earthquake", .free_tier = 1,
};
REGISTER_SOURCE(my_src);
```

There is **no** distinction in the ABI between a "JapanOSINT map collector" and
an "OSINTsaas service". Every registered source is schedulable when
`update_interval_sec > 0` and dispatchable by the OSINT search regardless.
Polymorphism is entirely `ctx->entity`: NULL on a scheduled feed run, set to the
pivot value on an OSINT dispatch.

Many files register sources through a local macro (`RSSX`, `RSS`, `DEF`, …)
that expands to a full `source_def` plus its own `REGISTER_SOURCE`, so
**`grep -c REGISTER_SOURCE` undercounts badly**. Get the real number from:

```sh
make -C native source-count          # macro-aware count
native/bin/japanosint --list-sources # what the built binary actually registered
python3 native/tools/lint_sources.py --list-ids
```

Curated per-source metadata (category, human name, licence, map layer) lives in
`native/core/source_registry.gen.c`; sources absent from that table carry their
metadata inline in the `source_def` and `src_meta_get()` synthesises it.

---

## 1. Collection pipeline

```
scheduler_loop            core/scheduler.c
  └─ per source: due?  ──▶ def->run(ctx, sink)      collectors/sources/*.c
                             │  HTTP via core/httpclient.c (curl)
                             │  parse: lib/rss_atom.c, lib/csv.c, cJSON, …
                             └─ sink->emit(&item)  for each record
                                   │
intel_sink::emit          core/intel.c
  ├─ derive uid            "<source_id>|<remote_key or content hash>"
  ├─ UPSERT intel_items    INSERT … ON CONFLICT(uid) DO UPDATE
  │                        (derived geometry — llm/exif — is preserved, not
  │                         overwritten by a collector's NULL lat/lon)
  ├─ FTS5 index            core/fts_schema.c + core/fts.c (MeCab segmentation
  │                        for Japanese; Latin passes straight through)
  ├─ simhash               core/simhash.c — near-duplicate detection
  ├─ content_change        core/content_change.c — what changed since last run
  ├─ entity extraction     core/entitystore.c — NER → entity graph
  └─ alert evaluation      core/alert_eval.c — rules fire on NEW rows only
                                   │
fetch_log + anomaly       core/scheduler.c, core/maint_detect.c
  └─ run outcome → status → anomaly_detect → collector_repair breaker
```

Two things about that last box are worth knowing, because they are the source
of the codebase's most expensive class of bug:

* `emit()` returns **1 for a new row, 0 for an update, <0 for an error**. The
  distinction is real (intel.c probes for the uid first) and the alert engine
  depends on it — rules must fire on new intel, not on every scheduled refetch.
* A source's `run()` return code is **not** a success/failure bit, and
  `scheduler.c` no longer treats it as one. A collector that returns non-zero
  because upstream had nothing new gets its `fetch_log.status` set to `error`,
  which opens a `status_bad` anomaly, which the repair pod triages, which after
  three cycles trips `collector_repair.c`'s breaker and **quarantines a working
  source**. This is why `return n > 0 ? 0 : -1` is a defect and why
  `make lint-sources` checks for it. An honest empty result returns 0.

### Adding a source

See `native/collectors/SOURCE_AUTHORING_CONTRACT.md` for the rules that are
enforced. The short version: real fetch or honest empty. Never invent a record,
a name, a coordinate or a value; a key-gated source with no key returns 0 rows
and 0 status. Any row carrying geometry must also carry a `geo_precision`
property saying how good that coordinate is — an exact fix and a prefecture
centroid must not be indistinguishable at the API boundary.

---

## 2. OSINT search pipeline

`core/pipeline.c` is a faithful port of the retired `server/src/osint/pipeline.js`.

```
query ─▶ analysis prompt (core/prompts.c, GBNF-constrained)
           │  llama-server /v1/chat/completions, grammars/osint_analysis.gbnf
           ▼
        {entity, entity_type, source_ids[…], plan}
           │
        core/osint_dispatch.c — the dispatcher IS the source registry:
           any registered id, run with ctx->entity set
           │  dual sink: persists live through the shared intel_sink AND
           │  captures the result JSON for phase 2
           ▼
        phase-2 synthesis prompt ─▶ answer + suggestions
           │  core/progress.c streams stage/percent to the client (SSE)
           ▼
        /api/search results + everything already persisted as intel
```

Grammar files live in `grammars/*.gbnf` (7 of them) and are resolved at runtime
against `JO_REPO_ROOT` — see [`BUILD.md`](./BUILD.md) for how that is derived
and for the one failure mode it still has.

---

## 3. Where it surfaces

`core/httpd.c` (mongoose) serves the REST API on `PORT` (default 4000). The
live client is the SwiftUI iOS app in `ios/`. There is no static-file handler:
every non-`/api` path 404s, so the React app in `client/` is not served by this
binary — see [`../client/README.md`](../client/README.md).
