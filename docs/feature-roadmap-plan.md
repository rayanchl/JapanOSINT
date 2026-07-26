# JapanOSINT — Feature Roadmap Implementation Plan

> **STATUS (2026-07-26).** Backend for P0, P1, and most of P2/P4 is built,
> wired and verified: 58 automated checks green (49 HTTP + 9 ingest→alert),
> run against a scratch DB with `JO_NO_SCHED`, never production data or the
> network. Harness: `gcc`/`make` under WSL Ubuntu; there is no compiler on the
> Windows host.
>
> Done: **7, 8, 10, 11, 14, 15, 18, 19(be), 22, 23, 30**, plus **P0.1/0.2/0.3**.
> In flight: **9, 16, 17, 21, 24, 25, 29, 38**.
> Not started: **26, 27, 28** (queued), **31–37** (map + iOS platform).
> **No iOS UI exists for any of it** — every item above is API-only.
>
> Two corrections to the original review are folded in below: item 6 was
> already implemented, and item 23 was half implemented.
>
> One fix not in the original plan: `intel_sink.emit()` documented itself as
> "1 if NEW, 0 if updated" but returned 1 for both halves of the upsert, so
> any rule with `dedup_window_sec=0` would have re-fired on every collector
> refresh forever. `core/intel.c` now probes for the uid first; there is a
> regression test for it.

Plan for items 6–38 of the feature review. Effort scale (solo dev):
**S** 1–2 d · **M** 3–5 d · **L** 1–2 wk · **XL** 3 wk+

Two findings that change the plan up front:

1. **P0 is not optional.** Items 9, 10, 11, 21 and 24 all assume an alert
   evaluator that does not exist. `alertsapi.c` is CRUD-only — nothing ever
   INSERTs into `alert_events` (only `DELETE` at :260 and `SELECT` at :374),
   and there is no SMTP or outbound-POST code anywhere in `native/core`.
   Five of the chosen items are unshippable until that engine exists, so it is
   scheduled first as **P0** even though it was not in the selected list.
2. **The iOS map is MapKit** (`Map/MapTab.swift:2` — `import MapKit`), not
   MapLibre. MapKit cannot do vector style layers, offline tile caching, or
   3D tilesets. Items 31, 33 and 34 are gated on a map-engine decision
   (see **F.0**) and are the only items in this plan with an architectural
   fork in front of them.

Corrections to the original review, verified in source:

- **Item 6 is already done.** `pipeline.c:437` writes the `pivot_discovered`
  edge and `entitystore.h` exposes a complete write surface. The "read-only,
  documented follow-up" note in `pipeline.h` is stale. Reduced to a docs fix
  plus edge-type surfacing (folded into 19/20).
- **Item 23 is half done.** `breach_index.c:307–318` upserts an entity and a
  mention per breach identifier, keyed `breach:<keyid>`. Since
  `es_upsert_entity` dedups on `(type, norm_key)`, breach and intel mentions
  of the same email are already one node. Reduced to read-side surfacing.

---

## Phase order and rationale

```
P0  alert engine (prereq)                    ──┐
P1  foundations: 14, 7, 20, 8                  │
P2  analyst workflow: 15, 17, 16, 18, 19       │
P3  alerting UX: 11, 9, 10  ───────────────────┘
P4  entity & breach: 23, 21, 22, 24, 25
P5  collection: 30, 26, 29, 27, 28
P6  map: F.0 decision → 32, 33, 31, 34
P7  iOS platform: 37, 35, 36
P8  38
```

**Item 14 (cases) is the spine.** 15, 16, 17, 18, 31, 36 and 38 all attach to
it; building any of them first means building them twice. **Item 7 (export)**
is the substrate for 16. **Item 20 (entity write API)** unblocks 19 and 22.

---

## P0 — Alert engine (prerequisite, not in the selected list)

Blocks 9, 10, 11, 21, 24.

**P0.1 — Evaluator.** New `native/core/alert_eval.c/.h`. Hook into the single
ingest chokepoint: `intel_sink_make`'s emit in `core/intel.c`, after upsert
and FTS mirror, same transaction. On a new row (`emit` returns 1), load the
tenant's enabled `alert_rules`, evaluate `predicate_json` against the item
(`q` via `intel_items_fts` MATCH, `source_ids`, `tags_any`/`tags_all` via
`json_each`, `bbox` against `lat`/`lon`), and INSERT into `alert_events`.
Enforce `dedup_window_sec` against `idx_alert_events_rule_item`,
`storm_cap_per_hour` against `idx_alert_events_rule_ts` (over-cap rows land
with `suppressed=1` and `reason='storm_cap'`), and `muted_until`.
Rules are cached in memory with a `updated_at` generation counter so a hot
ingest path is not doing a `SELECT` per item. **M**

**P0.2 — Delivery worker.** New `native/core/alert_deliver.c`. Background
thread draining undelivered `alert_events`, writing outcomes into
`delivered_channels_json`. Two transports:
- `webhook` — POST via the existing `http_client` (`core/httpclient.h`), body
  `{rule, event, item}`, signed `X-JO-Signature: sha256=<hmac>` using the
  already-stored-and-masked channel `secret` (`alertsapi.c:48`). Retry with
  exponential backoff, 5 attempts, then dead-letter.
- `email` — SMTP via libcurl (`CURLOPT_URL smtp://…`), or Postmark/SES HTTP
  API if you would rather not hold SMTP credentials. Config via `.env`.

Add a `alert_deliveries` table (`event_id, channel_idx, attempt, status,
http_code, error, attempted_at`) so retries and failures are inspectable —
`delivered_channels_json` alone cannot express "tried 4 times, 502". **M**

**P0.3 — Ops.** `/api/alerts/:id/test` fires a synthetic event through the
rule's channels so an operator can verify a webhook before trusting it. **S**

---

## P1 — Foundations

### 14. Cases — **L**

The spine. Replaces the flat starred list in `Saved/SavedStore.swift`.

New tables in `core/schema.sql`:

```sql
CREATE TABLE cases (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
  name TEXT NOT NULL, summary TEXT,
  status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','closed','archived')),
  priority INTEGER NOT NULL DEFAULT 0,
  created_by TEXT, created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT NOT NULL DEFAULT (datetime('now')), closed_at TEXT);

CREATE TABLE case_members (
  case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  role TEXT NOT NULL DEFAULT 'contributor' CHECK(role IN ('lead','contributor','viewer')),
  PRIMARY KEY (case_id, user_id));

-- Polymorphic pin. ref_type: intel_item | entity | breach_item | feature | camera | search_run
CREATE TABLE case_items (
  case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
  ref_type TEXT NOT NULL, ref_id TEXT NOT NULL,
  label TEXT, snapshot_json TEXT,        -- denormalized display copy; survives source deletion
  added_by TEXT, added_at TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (case_id, ref_type, ref_id));

CREATE TABLE case_activity (              -- feed + @mention target
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
  actor_id TEXT, kind TEXT NOT NULL,      -- created|pinned|noted|status|assigned|commented
  body TEXT, target_ref TEXT, mentions_json TEXT NOT NULL DEFAULT '[]',
  ts TEXT NOT NULL DEFAULT (datetime('now')));

CREATE INDEX idx_cases_tenant_status ON cases(tenant_id, status, updated_at DESC);
CREATE INDEX idx_case_items_ref ON case_items(ref_type, ref_id);
CREATE INDEX idx_case_activity_case ON case_activity(case_id, ts DESC);
```

Backend: new `core/casesapi.c/.h`, routed in `httpd.c` inside the `/api/`
auth gate alongside `/api/alerts`. Endpoints: `GET/POST /api/cases`,
`GET/PATCH/DELETE /api/cases/:id`, `GET/POST/DELETE /api/cases/:id/items`,
`GET/POST /api/cases/:id/activity`, `GET/PUT /api/cases/:id/members`.
Every mutation writes to `audit_events` via the existing hash-chained path —
cases are the thing an auditor will ask about.

`snapshot_json` matters: pinning must capture a display copy, because scraped
intel items get re-fetched and breach corpora get re-ingested. A pin that
renders as "deleted item" is worthless in a report.

iOS: new `ios/JapanOsintApp/Cases/` — `CasesTab.swift`, `CaseDetailView.swift`,
`CasePickerSheet.swift` (the "Add to case" sheet reachable from every detail
view), `CaseStore.swift`. Add a `.cases` entry to `AppTab` and to both
`RootView.tabView` and `RootView.sidebarList`. The phone TabView is already at
its 5-tab limit, so Cases replaces Saved as a top-level tab and Saved becomes
a smart case ("Unfiled") inside it — that keeps the tab count and gives the
existing `SavedFeature` rows a migration target.

Migration: one-time `SavedStore` → `case_items` under an auto-created
"Unfiled" case per tenant, keeping `SavedFeature` as the local mirror.

Dependencies: none. Everything else in P2 depends on this.

### 7. Export — **M**

New `core/exportapi.c/.h`. One generic exporter behind
`GET /api/export/:kind?format=csv|geojson|json&<existing filters>`, where
`:kind` ∈ `intel|entities|breach|case|alert_events` and the filter set is the
existing `intel_items_query` struct (`intelapi.h`) so filters and exports can
never drift apart. Streamed via mongoose chunked responses — a 2.9 GB DB will
produce exports that must not be built in memory.

- CSV: RFC 4180, UTF-8 BOM (Excel and Japanese text), configurable columns.
- GeoJSON: `FeatureCollection` from rows with `lat`/`lon`/`geometry`; reuse the
  existing GeoJSON emitters in `dataapi.c`.
- JSON: the same envelope the API already returns, cursor-paged to completion.

Row cap by `tenants.plan`; exports write an `audit_events` row (who exported
what, how many rows) — this is the single highest-risk data-egress path in the
product and it should not be silent.

iOS: `ShareLink`/`fileExporter` from Intel list, Entity profile, Case detail.

PDF is deliberately **not** here — it belongs to the report builder (16), not
to a generic row exporter.

### 20. Entity write API + merge/split — **M**

Correction applied: the write *surface* exists (`entitystore.h`). What is
missing is HTTP exposure and human override.

Extend `entityapi.c` (currently read-only) with:
- `POST /api/entities` — create/upsert (`es_upsert_entity`).
- `PATCH /api/entities/:type/:id` — canonical name, aliases, properties.
- `POST /api/entities/merge` `{a, b}` — `es_record_merge` +
  `es_union_entities`, with `reason='human'` and `confidence=1.0`.
- `POST /api/entities/split` — **this is the missing primitive.**
  `es_union_entities` is destructive (deletes the loser row, repoints mentions),
  so a merge cannot currently be undone. Add an `entity_unmerge_log` table
  capturing the loser's pre-merge `(entity_id, canonical, aliases, mention
  uids)` before the union, so split can replay it. Without this, "analysts
  will not trust an ER graph they can't correct" stays true even with a merge
  button.
- `POST /api/entities/:type/:id/relationships` — manual edge with
  `rel_type='asserted'`, so analyst-asserted links are distinguishable from
  `co_mention` and `pivot_discovered` in the graph canvas (19).

Gate on role `analyst`+; audit every mutation.

iOS: merge/split affordances in `Entities/EntityViews.swift`, a duplicate-
candidate queue reading `entity_merges` rows the LLM flagged but did not act on.

### 8. Repo hygiene — **S**

```
git rm --cached native/.run/*.pid native/bin/japanosint
```
Add to `.gitignore`: `native/.run/`, `native/bin/`. Note that `native/llama/*.so`
and `*.dylib` (~20 tracked files) are also build artifacts — the commit
`b371584` message calls them "prebuilt runtime libs", so if that is deliberate
they stay, but they belong in a release asset or Git LFS, not in tree history.
Do this first; it is 20 minutes and it stops every future diff from being noisy.

---

## P2 — Analyst workflow

### 15. Notes & annotations — **S**

```sql
CREATE TABLE annotations (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, case_id TEXT REFERENCES cases(id) ON DELETE CASCADE,
  ref_type TEXT NOT NULL, ref_id TEXT NOT NULL,
  body_md TEXT NOT NULL, author_id TEXT,
  created_at TEXT NOT NULL DEFAULT (datetime('now')), updated_at TEXT, deleted_at TEXT);
CREATE INDEX idx_annotations_ref ON annotations(ref_type, ref_id, created_at DESC);
```

`GET/POST/PATCH/DELETE /api/annotations`. Soft delete (`deleted_at`) — analyst
notes are evidence and hard-deleting them undermines 17. Same `ref_type`
vocabulary as `case_items`, so one polymorphic-ref helper serves both.

iOS: a reusable `AnnotationsSection` view dropped into `IntelDetail.swift`,
`EntityViews.swift`, and the map popups under `Map/popups/`. Markdown render
via `AttributedString(markdown:)`.

### 17. Evidence preservation / chain of custody — **L**

The most technically demanding item in P2 and the one with the most product
value for a paying analyst.

```sql
CREATE TABLE evidence (
  id TEXT PRIMARY KEY, item_uid TEXT NOT NULL, source_id TEXT NOT NULL,
  captured_at TEXT NOT NULL DEFAULT (datetime('now')),
  request_url TEXT, request_method TEXT, request_headers TEXT,
  response_status INTEGER, response_headers TEXT,
  content_sha256 TEXT NOT NULL, content_bytes INTEGER, content_type TEXT,
  blob_path TEXT NOT NULL,          -- $JO_EVIDENCE_DIR/<sha256[0:2]>/<sha256>
  prev_hash TEXT, row_hash TEXT, chain_seq INTEGER);
CREATE INDEX idx_evidence_item ON evidence(item_uid, captured_at DESC);
CREATE UNIQUE INDEX idx_evidence_sha ON evidence(content_sha256, item_uid);
```

Capture at the one place every fetch already passes through: `http_client`
in `core/httpclient.c`. Add an opt-in `evidence_sink` on `source_ctx` so a
collector's raw response body is written content-addressed to disk and
SHA-256'd before parsing. Content-addressed storage means the 20 sources that
report the same earthquake share one blob.

Chain the rows with the same `prev_hash`/`row_hash`/`chain_seq` scheme already
used by `audit_events` (see `/api/audit/verify` in `httpd.c:825`) — reuse that
code path rather than reimplementing it, and extend `/api/audit/verify` to
verify the evidence chain too.

Storage governance is mandatory before this ships: at 415 sources on schedules
down to 60 s, uncapped capture will fill a disk in days. Ship with per-source
opt-in (a `capture_evidence` column on `sources`), a global byte budget, and
an LRU reaper that never evicts a blob referenced by a `case_items` pin.

`GET /api/intel/items/:uid/evidence`, `GET /api/evidence/:id/raw` (operator-
gated via `opgate_check`, audited).

iOS: an "Evidence" section in `IntelDetail.swift` — capture time, SHA-256,
HTTP status, verify-chain badge, and a raw-view sheet.

### 16. Report builder → PDF/Markdown — **L**

Depends on 14, 15, 7, and reads 17.

`POST /api/cases/:id/report` `{format: md|pdf|html, sections:[…]}` in a new
`core/reportapi.c`. Assemble: header (case, author, generated-at, tenant),
executive summary, findings from `case_items` in pin order with their
annotations, entity graph, source citations with capture timestamps and
evidence hashes, and an audit trail excerpt.

Markdown first — it is a string builder over data you already have, and it is
the honest 80% of the value. HTML is a template over the same model.

For PDF, do **not** link a PDF library into the C server. Emit HTML and render
it out-of-process (headless Chromium/`weasyprint`) behind a small job queue
reusing the `breach_jobs.c` pattern (`/api/admin/breach/jobs` already
demonstrates job-status polling in this codebase), or render client-side on
iOS from the HTML via `UIPrintPageRenderer`/`WKWebView.createPDF`. The
iOS-side render is the lower-risk path and keeps the server dependency-free.

Report templates per tenant (logo, classification banner, footer) are the
obvious paid-tier hook.

### 18. Timeline view — **M**

Backend: `GET /api/timeline?since&until&bucket=hour|day&case_id&source&bbox`
in `core/timelineapi.c`. Union three streams into one sorted, bucketed
response: `intel_items` (by `COALESCE(published_at, fetched_at)`),
`entity_mentions` (by `created_at`), `alert_events` (by `matched_at`).
Return both bucket counts (for the histogram) and a keyset-paged event list,
reusing the base64url cursor scheme from `intelapi.h`.

iOS: `Timeline/TimelineView.swift` — a histogram strip plus a scrubbable
event list. Wire it to the existing `Map/PlaybackState.swift` and
`Map/TimeSliderView.swift` so scrubbing the timeline drives the map's time
window and vice versa. Those two files are the reason this is M and not L —
the temporal state machine already exists.

### 19. Link/graph canvas — **L**

Backend: mostly done. `entityapi_graph(db, type, id, depth)` exists. Add
`rel_type` and `weight` to the edge payload if absent, plus a
`?rel_types=co_mention,pivot_discovered,asserted` filter and an
`?exclude_hubs=N` guard — an ego-network around a high-degree entity (a
prefecture, say) will otherwise return thousands of edges and the client will
die. Also fix the stale `pipeline.h` comment while you are in there.

iOS: `Entities/GraphCanvasView.swift`. Force-directed layout on a `Canvas`
with a `TimelineView(.animation)` driver — no third-party dependency needed at
this node count. Interactions: tap to expand a node (fetch its ego-network and
merge), long-press to pin, double-tap to open the profile, edge styling by
`rel_type` (solid `asserted`, dashed `co_mention`, arrowed `pivot_discovered`),
edge thickness by `weight`. "Add all visible to case" writes to `case_items`.

Cap at ~300 nodes with a "showing 300 of N" affordance rather than silently
truncating.

---

## P3 — Alerting UX

All three require P0.

### 11. Notification inbox — **S**

Depends on P0.1. `GET /api/alert-events?unread=1&cursor=` plus
`POST /api/alert-events/:id/read` and `POST /api/alert-events/read-all`. Add
`read_at TEXT` to `alert_events`.

iOS: extend `Alerts/AlertEventsView.swift` into an inbox — unread badge on the
tab, swipe-to-read, swipe-to-mute-rule (PATCH `muted_until`), tap-through to
the matched item. Badge count comes from a lightweight `GET
/api/alert-events/unread-count` polled on foreground.

### 9. AOI geofence alerts — **M**

Depends on P0.1. `predicate.bbox` is already validated as `[w,s,e,n]`
(`alertsapi.c:~118`) but nothing draws it and polygons are unsupported.

Backend: extend the predicate to accept `{bbox:[w,s,e,n]}` **or**
`{polygon:[[lon,lat],…]}` **or** `{circle:{lat,lon,radius_m}}`. Evaluation in
`alert_eval.c`: bbox test first (cheap, index-assisted via
`idx_intel_items_geom`), then exact point-in-polygon / haversine as a
refinement. Reuse `core/linegeom.c` for the geometry primitives.

Optionally persist named AOIs (`areas_of_interest` table: id, tenant_id, name,
geometry, created_by) so one shape can back several rules and can also be
drawn as a map layer.

iOS: draw mode in `Map/MapTab.swift` — tap-to-place vertices over the MapKit
map with an `MKPolygon` overlay, "Alert me here" → prefilled `AlertEditor`.
MapKit supports overlay rendering fine; only *styled vector* layers are the
problem, and this is not that.

### 10. Rule dry-run / backtest — **S**

Depends on P0.1 (shares the predicate evaluator).
`POST /api/alerts/preview` `{predicate, since}` → runs the same matcher over
`intel_items` for the window and returns match count plus the top 50 matches,
without writing `alert_events`. Cap the window at 30 days and the scan by
`idx_intel_items_tenant_fetched`.

iOS: a live "would have matched N items in the last 7 days" line in
`Alerts/AlertEditor.swift`, debounced, with an expandable sample list.

---

## P4 — Entity & breach intelligence

### 23. Breach ↔ entity pivot — **S** (was M; correction applied)

The ingest-side link already exists. Remaining work is read-side:
- `GET /api/entities/:type/:id/breaches` — join `entity_mentions`
  (`extractor='breach-ingest'`) → `breach_items` → `breach_meta` for the
  breach name, date, `pwn_count` and `data_classes_json`.
- Add an exposure summary to `entityapi_get`: breach count, earliest and
  latest breach date, aggregated data classes.

iOS: an "Exposure" section on the entity profile; a tap-through from a breach
record's entity chip (already served by `entityapi_item_entities`) to the
entity profile and back. This closes the loop that is currently one-directional.

### 21. Entity watchlists — **M**

Depends on P0. Rather than a parallel subscription system, express a watchlist
as an `alert_rules` row with a new predicate key
`{entity_ids:["…"], entity_types:[…]}`, evaluated in `alert_eval.c` against
the `entity_mentions` written for the item during the same ingest transaction.
One engine, one delivery path, one inbox.

Add `watchlists (id, tenant_id, name, entity_ids_json, rule_id, created_by)`
only as sugar over rule creation, so a user manages "who I'm watching" without
seeing predicate JSON.

iOS: a "Watch" toggle on the entity profile; a "Watching" section in the
Alerts tab.

### 22. Correlation scoring — **M**

`entity_relationships.weight` is a raw accumulated count. Compute significance
offline in a new `core/entity_stats.c`, run as a scheduled internal pod (the
`_maint`/`_enrich` convention in `scheduler.c:~56` already exists for exactly
this kind of non-collector job):

```
PMI(a,b) = log( P(a,b) / (P(a) · P(b)) )     over co-occurrence in item_uid
lift(a,b) = P(a,b) / (P(a) · P(b))
```

Counts come from `entities.mention_count` and `entity_relationships.weight`.
Store `pmi REAL`, `lift REAL`, `stats_at TEXT` on `entity_relationships`.
Suppress pairs below a support floor (say co-count < 3) — PMI is wildly
unstable in the tail and will otherwise surface noise as the strongest signal.

Surface as "40× above baseline" on the relationship row and as edge thickness
in the graph canvas (19). Sort the entity profile's related-entities list by
lift rather than raw weight.

### 24. Breach exposure monitoring — **M**

Depends on P0, 23.

```sql
CREATE TABLE breach_monitors (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, kind TEXT NOT NULL CHECK(kind IN ('email','domain','username','phone')),
  value_hash TEXT NOT NULL,       -- SHA-1, matching breach_items.hash; never store the plaintext
  label TEXT, rule_id TEXT, created_by TEXT,
  created_at TEXT NOT NULL DEFAULT (datetime('now')), last_checked_at TEXT);
CREATE INDEX idx_breach_monitors_hash ON breach_monitors(value_hash);
```

Store the hash, not the address — a monitor list of customer emails is itself
a breach target. For domain monitors, match on the value suffix, which needs a
`value_domain` generated column on `breach_items` plus an index (a `LIKE
'%@domain'` scan over that table is not viable).

Hook: at the end of `breach_index_ingest`, diff the newly written keyids
against `breach_monitors` and raise an `alert_events` row per hit through the
P0 engine. `breach_jobs.c` already gives you the job scaffolding.

iOS: a "Monitors" screen; on a hit, the notification deep-links to the breach
record with the secret still redacted behind the existing operator reveal path.

### 25. Cross-source dedup / clustering — **L**

Add `simhash INTEGER, cluster_id TEXT` to `intel_items`. Compute a 64-bit
simhash over the segmented title+body at ingest in `core/intel.c` (MeCab
segmentation is already in that path for the FTS mirror, so tokens are free).
Cluster by Hamming distance ≤ 3 using a 4-band index table
(`simhash_bands(band_idx, band_val, uid)`) — a full pairwise scan over this
corpus is not an option.

Assign `cluster_id` to the earliest-published member. Add `?collapse=1` to
`intelapi_list_items` returning one row per cluster with a
`duplicates: [{source_id, uid}]` array — that array is a *feature*, not
noise: "20 sources independently reported this" is corroboration, and showing
it is more valuable than hiding it.

Backfill existing rows in a batched migration; expect this to be the
longest-running migration in the plan.

---

## P5 — Collection depth

### 30. Source trust / freshness score — **S**

All inputs exist: `fetch_log` (status, records, duration), `collector_anomaly`
(verdicts, escalation), `sources.quarantined_at`, `sources.last_success`.

Compute in `core/statusapi.c` (it already aggregates per-source health):

```
reliability = 0.4·success_rate_30d
            + 0.3·freshness(last_success vs update_interval)
            + 0.2·(1 − anomaly_rate_30d)
            + 0.1·volume_stability(records stddev/mean)
quarantined → hard 0
```

Expose as `reliability` (0–1) plus a coarse `grade` (A–F) on `/api/status` and
`/api/intel/sources`. Show as a badge in `Dashboard/SourceDashboardTab.swift`,
next to every result in `Intel/IntelSourceRow.swift`, and as a sort key.

### 26. Content change detection — **M**

`maint_detect.c` detects collector *failure*. This detects content *change*.

```sql
CREATE TABLE content_snapshots (
  source_id TEXT NOT NULL, ref_key TEXT NOT NULL,   -- url or item uid
  content_sha256 TEXT NOT NULL, extract_text TEXT,
  captured_at TEXT NOT NULL DEFAULT (datetime('now')),
  PRIMARY KEY (source_id, ref_key, content_sha256));
```

On each fetch of a `type='scraped'` or `'web_request'` source, hash the
normalized extracted text (strip scripts, styles, timestamps, session tokens,
ad slots — without normalization every page "changes" every fetch and the
feature is pure noise). On a hash change, emit an intel item with
`record_type='content_change'` carrying a unified diff in `properties`.

Shares storage and the capture hook with 17 — build them adjacent, and 26
becomes largely free once 17's `evidence` blob store exists.

iOS: diff view in `IntelDetail.swift`, additions and deletions colored.

### 29. Backend translation at ingest — **M**

Today translation is iOS-side only (`Settings/SettingsTab.swift` →
`translationSection`), so FTS search and alert predicates remain Japanese-only.
That is the real cost: an English-speaking analyst cannot write a working
alert rule.

Add `title_en, body_en, summary_en, translated_at, translate_failed` to
`intel_items`. Translate in a worker pod following the `entity_enrich.c`
pattern (it is the closest existing analogue: batch, backoff, failure column),
using the local llama runtime via `core/llm.h` — you already run it for
NER and triage, so there is no new dependency and no per-token cost.

Mirror the English fields into `intel_items_fts` so one query hits both
languages. Add `?lang_view=en|ja|both` to the items endpoint.

Backfill is large: gate it behind an admin job with a progress endpoint
(`core/progress.h` already provides the SSE progress machinery).

### 27. Media pipeline: EXIF, OCR, pHash — **L**

New `native/collectors/sources/media_enricher.c` (an internal `_media` pod).
Triggered on items whose `properties` carry an image URL.

- **EXIF** — a small parser or libexif; extract GPS (which can geolocate an
  item that has no `lat`/`lon`, filling a real gap given
  `idx_intel_items_geom_pending`), camera make/model, timestamp.
- **pHash** — 64-bit DCT perceptual hash for near-duplicate image detection;
  same band-index trick as 25.
- **OCR (Japanese)** — the hard part. Tesseract with `jpn`+`jpn_vert`
  traineddata is the pragmatic choice; accuracy on camera stills and signage
  will be mediocre and should be labeled as machine-read, not asserted.

Store in a `media_assets` table (`item_uid, url, sha256, phash, exif_json,
ocr_text, ocr_conf, analyzed_at`) and mirror `ocr_text` into FTS so text in
images becomes searchable — that is the payoff.

Ship EXIF+pHash first (**M**), OCR second (**M**); they are independent.

### 28. Scheduled camera stills — **M**

Depends on 27 for the analysis half.

`camera_store.c` and the `cam_*` collectors already resolve feeds
(`Cameras/CameraFeedResolver.swift` handles the client side). Add a capture
worker: for cameras flagged `capture_enabled`, pull a frame on an interval
(ffmpeg for RTSP/HLS, plain GET for MJPEG/snapshot URLs), store
content-addressed alongside 17's blob store, row in
`camera_stills (camera_id, captured_at, sha256, blob_path, phash, ocr_text)`.

Retention policy is mandatory (N days or M frames per camera) — this is the
single fastest way to fill a disk in the entire plan.

Payoff: a camera becomes diffable (pHash delta → "scene changed"), a time
series, and a timelapse. iOS: a stills strip and scrubber in
`Cameras/CameraFeedView.swift`.

---

## P6 — Map & geospatial

### F.0 — Map engine decision (blocks 31, 33, 34) — **decision, then L–XL**

`Map/MapTab.swift:2` imports **MapKit** and uses SwiftUI `Map` (:261). MapKit
cannot do: offline tile caching (31), vector style layers with per-layer
opacity/ordering (33), or 3D tilesets (34). Item 32 is unaffected.

Three options:

| | Effort | Gets you |
|---|---|---|
| **A. Stay on MapKit** | — | 32 only. 33 degrades to overlay opacity; 31 and 34 are off the table. |
| **B. Adopt MapLibre Native iOS** | L (2 wk port) | 31, 33, 34 all become tractable. The React client already uses MapLibre, so styles are shareable. Cost: rewrite `MapTab`, all of `Map/popups/`, `LiveVehiclesOverlay`, `PlaybackState`. |
| **C. MapKit + MapLibre-in-WKWebView for 3D only** | M | 34 in isolation, without the port. Two map stacks to maintain — acceptable as a spike, bad as an end state. |

**Recommendation: B**, scheduled as its own project before 31/33/34, and only
once P1–P4 have shipped. It is the single largest refactor in this plan and it
buys nothing an analyst can see on its own. Do not start it early.

### 32. Spatial tools — **M** (no engine dependency)

- Measure distance/area: pure client work in `MapTab.swift`, MapKit overlays.
- "Everything within X of here": `GET /api/intel/items?near=lat,lon&radius_m=`
  — bbox prefilter on `idx_intel_items_geom`, then haversine refine. Same
  shape as 9's evaluation, so share the helper.
- **GTFS isochrone** — the standout. `gtfs_stop_times`, `gtfs_trips`,
  `gtfs_calendar` and `gtfs_shapes` are fully populated, so a
  Dijkstra/RAPTOR-lite over stop→stop edges with a walking-transfer radius
  gives real travel-time reachability. New `core/isochrone.c`,
  `GET /api/isochrone?lat&lon&max_min&depart_at` → GeoJSON polygons per band.
  Cache aggressively in `collector_cache` (a given origin/time-bucket is
  stable). This is a genuinely differentiating OSINT primitive and nothing
  else in the plan is close to it for effort-to-value.

### 33. Custom layers + styling — **M** (A) / **L** (B)

Upload GeoJSON/KML/CSV-with-coords → `user_layers (id, tenant_id, name,
format, feature_count, geojson_blob, created_by)`, served through the existing
`/api/layers` and `/api/data/:layer` contract so custom layers are
indistinguishable from built-ins downstream. Per-layer opacity, color and
z-order persisted per user.

Under MapKit (A), opacity and ordering work on overlays; full style control
needs B.

### 31. Offline bundle — **L**, requires engine B

`Shared/OfflineStateView.swift` handles the offline *error state*; there is no
offline *data*. With MapLibre: download an MBTiles/PMTiles region for a
bbox, plus a case's pinned items, annotations and evidence blobs, into a
signed bundle. iOS: `Offline/OfflineBundleManager.swift` over SwiftData.
Bundle size and expiry must be visible in Settings, and bundle export must be
audited — it is data egress by another name.

### 34. PLATEAU 3D + viewshed — **XL**, requires engine B or C

`/api/plateau/tilesets` (`httpd.c:678`) already serves tilesets. Render 3DTiles
via MapLibre, then add viewshed: sample rays from an observer point against
PLATEAU building heights + GSI elevation (`gsi-elevation` is already a
registered source) → visible/occluded polygon. Line-of-sight from a camera
position is the real OSINT capability here.

Genuinely differentiating, and correctly last: it depends on the map port,
elevation ingest, and a nontrivial geometry kernel.

---

## P7 — iOS platform

### 37. Background refresh — **M**

`AppDataContainer`/`IntelCache` exist, but with no `BGTaskScheduler` the cache
is never fresher than the last foreground session.

Register `BGAppRefreshTask` ("app.refresh.intel", ~15 min best-effort) and
`BGProcessingTask` for larger syncs. Delta-sync using the existing keyset
cursor from `intelapi.h` — store `lastSyncCursor` per source and request only
what is newer. Respect the existing cellular-refresh settings in
`SettingsTab.swift → networkRefreshSection`.

Do this **before** 35: a widget backed by a stale cache is worse than no widget.

### 35. Widgets + Live Activities — **M**

New WidgetKit extension target.
- Small/medium widget: latest quake / active warnings / unread alert count,
  fed by an App Group shared container that the P7.37 background task writes.
- Lock Screen accessory: unread alert badge.
- Live Activity: a running OSINT search (`Search/PipelineView.swift` and
  `core/progress.c` already model per-round progress — that maps directly onto
  a Live Activity's content state), and active JMA warnings.

Depends on P0 for anything alert-driven, and on 37 for freshness.

### 36. Spotlight + App Intents + Share extension — **M**

- **CoreSpotlight**: index saved items, cases and entities with
  `CSSearchableItemAttributeSet`; handle `CSSearchableItemActionType` in
  `JapanOsintApp.swift` to deep-link. Requires a URL scheme / universal-link
  router, which item 38 also needs — build the router once, in whichever
  lands first.
- **App Intents**: `SearchJapanOSINTIntent`, `CheckExposureIntent`,
  `AddToCaseIntent`. Donate for Siri and Shortcuts.
- **Share extension**: accept URL / image / text → start an OSINT search or
  attach to a case. Attaching an image should feed the media pipeline (27).

---

## P8

### 38. Saved searches, history sync, permalinks — **M**

```sql
CREATE TABLE saved_searches (
  id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, user_id TEXT NOT NULL,
  name TEXT, kind TEXT NOT NULL CHECK(kind IN ('intel','osint','entity','breach','map')),
  params_json TEXT NOT NULL, pinned INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT (datetime('now')), last_run_at TEXT, run_count INTEGER NOT NULL DEFAULT 0);
CREATE TABLE search_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT, tenant_id TEXT NOT NULL, user_id TEXT NOT NULL,
  kind TEXT NOT NULL, params_json TEXT NOT NULL, result_count INTEGER,
  ts TEXT NOT NULL DEFAULT (datetime('now')));
```

`GET/POST/DELETE /api/saved-searches`, `GET /api/search-history`.
`Search/SearchStore.swift` is currently device-local; point it at these.

**Permalinks**: one canonical state encoder shared by web and iOS —
`jo://…` / `https://<host>/s/<b64url-state>` covering search params, map
camera + active layers + time window, entity id, case id. Resolve in
`JapanOsintApp.swift` via `onOpenURL`. This is the same router 36 needs.

Natural upsell: a saved search becomes an alert rule in one tap (`params_json`
→ `predicate_json`), which is the cleanest funnel from casual use into the
paid alerting feature.

---

## Summary

| Phase | Items | Effort |
|---|---|---|
| P0 prereq | alert evaluator, delivery, test-fire | ~2 wk |
| P1 foundations | 14, 7, 20, 8 | ~3 wk |
| P2 workflow | 15, 17, 16, 18, 19 | ~5 wk |
| P3 alerting UX | 11, 9, 10 | ~1.5 wk |
| P4 entity/breach | 23, 21, 22, 24, 25 | ~4 wk |
| P5 collection | 30, 26, 29, 27, 28 | ~5 wk |
| P6 map | F.0 decision, 32, 33, 31, 34 | ~2 wk (32/33 only) + 6–8 wk if engine B |
| P7 iOS platform | 37, 35, 36 | ~3 wk |
| P8 | 38 | ~1 wk |

**Shortest path to a sellable product**: P0 → 14 → 7 → 15 → 16. That is
roughly six weeks and it converts the app from a live map into something an
analyst can produce a billable deliverable from. Item 32's isochrone is the
best effort-to-differentiation ratio in the whole plan and can be slotted in
independently at any point.

Three items to schedule with care: **17** (evidence) needs storage governance
before it ships or it will fill a disk; **25** (dedup) carries the longest
backfill; **F.0** is a two-week refactor that no user can see and should not
start until P1–P4 have landed.
