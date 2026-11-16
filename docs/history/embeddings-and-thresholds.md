<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Embedding pipeline, vector indexes, thresholds registry and measurement, tenancy, CORS. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

## Embedding pipeline and tenancy (audit of 2026-10-02)

`embedding-backfill` (core/embed_pod.c) embeds new and changed rows into
`intel_vec_v2`; `/api/intel/semantic` queries it on a worker thread, never the
event loop. What it guarantees now, each pinned by `tests/unit/test_embed_pod.c`:

* Re-fetched rows whose text did not change are marked seen, and the delta walk
  resumes from a saved keyset — it used to rescan the same first 2,000 uids
  every tick and stop embedding new rows for good. `JO_EMBED_MAX_PER_RUN`
  counts rows SENT; `JO_EMBED_MAX_WALK` bounds rows looked at.
* A batch the server refuses is retried row by row; a row that fails alone goes
  to `intel_vec_failed` and is skipped until its text changes. Coverage reports
  `failed_count` and a sample. Text is cleaned to valid UTF-8 before sending.
* The model is identified from `/v1/models` and stored only with the first
  successful write; a failed detection skips the tick. A same-dimension model
  swap is refused by the pod AND by queries (503), and `service_vec` uses the
  same identity rule.
* A row's date is `published_at` only when it is ISO, else `fetched_at` — epoch
  and dd/mm/yyyy dates used to make rows ineligible and uncounted.
* Distance is L2. llama-server returns unit vectors, so the ranking equals
  cosine (d² = 2 − 2cos).

**One vector library, several indexes (2026-10-12).** `lib/vecindex.{c,h}` is
the only implementation of model identity, dimension probe, incremental upsert
by (key, text hash), KNN with metadata filters, coverage and RRF fusion; every
index below is built on it and listed under `vectorIndexes` in `/api/status`.
**Never `ALTER … RENAME` a vec0 table** (its shadow tables do not follow), and
never put two models' vectors in one index — image vectors have their own.

| index | holds | read by |
| --- | --- | --- |
| `intel_vec_v2` | items; `tenant_id` partition, `source_id`/`ts`/`record_type` filter INSIDE the search (the old `intel_vec` is migrated by copy, not re-embedded) | `/api/intel/semantic` (+filters), `sort=hybrid`, `/items/:uid/similar`, `/cases/:id/related`, Nearby `q`, `/timeline?q=`, the corpus lookup, prompt record ranking, stories/novelty, semantic alerts |
| `service_vec` | every registered source's card (kind pivot/scheduled/pod, aliases from `core/service_aliases.c`), incremental | phase-1 and per-round routing (pivots only), `/api/sources/semantic`, `service_vec_route_sources`, canonical category votes, layer review, `batch_exclusions.py --semantic` |
| `entity_vec` | entity name + romaji, partitioned by type | merge candidates for the LLM judge (nothing auto-merges), `/api/entities/search?mode=hybrid` |
| `fewshot_vec`, `anomaly_vec` | the analysis examples (`grammars/fewshot_analysis.jsonl`), past anomalies | per-query few-shot selection, triage memory |

`embed_hook_register()` (core/embed_pod.h) runs after each committed batch
with the vectors in hand — stories (`story_id`, separate from simhash's
`cluster_id`), `novelty`, and natural-language alert rules are evaluated
there, never by an embed call on the ingest path. **Every similarity threshold
lives in ONE registry, `core/thresholds.{c,h}`** (2026-10-21), and is read
through `threshold_get("<key>")` — never a module-local `getenv()` or a bare
constant at the comparison. Resolution per key: a runtime override in
`settings_thresholds` > its env var (the same names as before; an unparsable or
out-of-range value is ignored, as it always was) > the module's compiled
constant, which stays in its header as the documented default. The DB layer is
cached in memory (loaded by `threshold_migrate()` from `db_open`, refreshed on
every write), so a hot path never touches SQLite. Eleven keys:
`entity_vec_cos_floor` 0.80, `story_tau` 0.85, `alert_sem_threshold` 0.82 (a
rule's own `threshold` still wins), `relevance_sim_min` 0.40,
`suggest_min_cos` 0.50, `resolve_emb_min_cos` 0.60, `resolve_emb_margin` 0.10,
`camera_dup_cosine_min` 0.97, `repair_sem_cos_floor` 0.70 and
`corpus_vec_max_distance` √2 (an L2 distance — lower is stricter) and
`route_min_cos` 0.55 (which scheduled sources a question with no entity is
routed to — every one at or above it, no count cap). Operators
move them live, without a restart, through `GET /api/admin/thresholds` (value,
`source` db|env|default, default, min, max, env) and
`PUT /api/admin/thresholds/:key` `{value}` | `{reset:true}` — platform-operator
gate, a value outside [min,max] is a 400 not a clamp, every change an
audit-chain row (`threshold.set` / `threshold.reset`, old and new value) — from
Admin › Thresholds in the web client and Admin › Similarity thresholds on iOS.
**A new similarity cut-off gets a row in that table**, not its own getenv.
**THEY HAVE NOW BEEN MEASURED ON THE PRODUCTION MODEL** (2026-10-23 — this
paragraph said "none has been measured" until then, and the values had been
chosen from typical multilingual-e5 scores and tested against a stub). A real
server was stood up for it: llama.cpp built from source and
`multilingual-e5-large-q8_0.gguf`, the file this tree's own notes name as the
production model, answering `/v1/embeddings` with 1024-d unit vectors and
WITHOUT the e5 `query:`/`passage:` prefixes, because `core/service_vec.c`
measured those worse here and defaults them off. Full readings:
`docs/measured-thresholds-2026-10-23.{md,tsv}`. Three results:

* **`story_tau = 0.85` is right, and now measured.** 200 positive pairs (two
  items sharing a simhash `cluster_id`) against 200 negatives (different
  cluster AND different source) separate COMPLETELY: positives 0.9395 at the
  10th percentile and 1.0 at the median, negatives topping out at 0.8372. Best
  F1 cut-off 0.8372 at F1 = 1.000, and the value in force achieves precision
  1.000 and recall 1.000 with headroom on both sides.
* **The UNRELATED band on this model starts at 0.69, and SIX of the eleven
  cut-offs are below it.** 300 question ↔ unrelated-record pairs across 30
  sources score **0.6874 to 0.8448** (median 0.7641) — so `relevance_sim_min`
  0.40, `suggest_min_cos` 0.50, `route_min_cos` 0.55 and `resolve_emb_min_cos`
  0.60 admit **100%** of unrelated pairs, `repair_sem_cos_floor` 0.70 admits
  99.3%, and `corpus_vec_max_distance`'s L2 ceiling of √2 admits 100% (the
  observed unrelated L2 range is 0.5572–0.7906, so √2 is twice the maximum).
  `route_min_cos` was re-measured in its OWN space — a question against a
  source CARD, not a record — and the minimum of 300 such pairs is 0.7261
  against a cut-off of 0.55, so a question with no entity routes to every
  scheduled source. `entity_vec_cos_floor` 0.80 admits 14% and
  `alert_sem_threshold` 0.82 admits 2%: those two are real filters.
* **What that does NOT license.** It says a cosine cut-off on this model must be
  **above ~0.85** before it rejects anything, because that is where the
  unrelated band ends (0.8448) and the known-same band begins (0.9395). It does
  not say what each value should be: that needs positives — pairs a human marked
  "these two SHOULD match" — which exist nowhere in this tree, and the tool
  reads them from a `--labels` file exactly so nobody invents them. **Nothing
  was changed.** `docs/threshold-labels.jsonl` is the next step and it is human
  work.

Running the harness for real also found three defects in it, the worst being
that **its negative set was EMPTY on a real database**: `pairs_story` read a
prefix `order by cluster_id` and then required two different sources, and on a
412,521-row database holding 31 sources every row in that prefix came from ONE
source — `n_neg = 0`, "ONE CLASS ONLY", after a full embedding run. A sample
taken off the head of a collection is not a sample of the collection, which is
the same error this tree recorded the same day for a record key measured on an
800-row sample. It samples per source now. Also: `in_force` was blank on every
key because nothing resolved the DEFAULT MACRO out of `core/thresholds.c`, so
the two columns that judge the value we ship could not be computed at all; and
only **six of the eleven defaults are in a header** — the other five are in
`core/thresholds.c` itself — which this paragraph used to assert the other way
round.

**The harness itself** (2026-10-22,
`tools/measure_thresholds.py`): it reads the eleven keys out of
`core/thresholds.c` rather than copying the table, asks a real server on the
same `/v1/embeddings` contract `core/llm.c` uses, and reports each key's
positive/negative score distributions, the best-F1 cut-off, and the precision
and recall the value IN FORCE achieves. It refuses to run without a real
server and says why — a stub is what produced the numbers it exists to
replace. Three keys have a label that is not circular (`story_tau` from
simhash `cluster_id`, since `story_id` was assigned BY `story_tau`;
`entity_vec_cos_floor` from `norm_key`; `camera_dup_cosine_min` from
`sha256`); the other eight judge a query against a record, no stored human
verdict exists, and they read UNLABELLED with a distribution only. No label
file is shipped on purpose: invented verdicts fed to the tool would turn the
guess into a "measurement". `resolve_emb_margin` is a GAP between a best match
and a runner-up, so it cannot be measured from pairs at all and says so.
`--self-test` runs from `make unit` and pins the one error that inverts a
reading silently — that an L2 distance is swept the opposite way to a cosine.
Without `JO_EMBED_URL` every feature falls back to its pre-embedding
behaviour byte for byte; that is pinned per feature in `tests/unit/`.

Tenancy: OSINT records are one shared corpus (tenant `legacy`, uid
`source|key`). A search run is not: its summary row is written under the tenant
that started it, `/api/intel/items`, `/search` and `/items/:uid` read "this
tenant + the shared corpus", and `/api/search/results` plus the SSE stream
answer the owner tenant (or the run's `stream_key`) only.

Workspace = tenant, and **everything a workspace authors is readable by all of
its members** (decided 2026-10-05): OSINT runs and their syntheses
(`GET /api/search/runs`), saved searches, search history, case notes. Reads
carry `user_id` + `mine` and `meta.scope`; `?mine=1` narrows a list (and its
`total`) to the caller. Edits, deletes and clearing history stay with the
author. Nothing crosses workspaces — `test_search_runs_workspace.c` and
`test_saved_search_workspace.c` pin both halves. A case VIEWER reads only;
content writes are analyst+ or the case's lead or a contributor
(`test_case_roles.c`). The case list and the case detail both carry the
caller's `my_case_role`, so clients gate on that same rule.

Search history holds every COMMITTED search (2026-10-09): the clients
`POST /api/search-history` on submit / opening a result / a committed URL's
results — never per keystroke, since `/api/entities/search` serves a
search-as-you-type box — and `searchapi.c` records an OSINT run at start.
The same search by the same person within `SS_HISTORY_MERGE_SEC` refreshes one
row instead of copying it. Do not record from the search routes themselves. List routes answer
`page:{limit,offset,count,total,has_more}`, `total` measured under the same
filters as the rows.

CORS is an explicit allow-list or nothing: `JO_CORS_ORIGINS` (comma-separated
exact `scheme://host[:port]`) is echoed with `Vary: Origin` and preflights are
answered before auth; unset, no CORS header is sent at all. It used to be a
hard-coded `*` on every reply — a cross-origin deployment of the web client
(`VITE_API_HOST`) must now set the variable. Local dev goes through the Vite
proxy and needs nothing.

