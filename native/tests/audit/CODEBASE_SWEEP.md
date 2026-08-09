# OSINTsaas — full codebase sweep

**2026-08-02 · 10-agent swarm · ~208k lines of first-party code**
`native/core` 50.8k · `native/collectors` 100k / 584 files · `native/lib` 3.4k ·
`client/src` 14.7k · `ios/` 38.7k

Method: ten investigators, one per subsystem, read-only (no edits, so they could
not collide). Each was given the prior source-audit findings so budget went to
new ground. Several verified their claims by executing them — ASAN harnesses,
a live HTTP server, an `emit()` microbenchmark, and read-only profiling of the
production database. Claims below are marked **VERIFIED** where I reproduced
them myself, **MEASURED** where an agent executed it, **STATIC** where it is a
reading of the code.

---

## 0. The one-line fix worth doing before anything else

**VERIFIED on the live 4.76 GB database, 2,743,208 rows:**

```
bare indexed column      0.002 s   SCAN … USING INDEX idx_intel_items_fetched
handler's ORDER BY      35.104 s   SCAN intel_items + USE TEMP B-TREE FOR ORDER BY
```

`intelapi.c:300` orders by `COALESCE(published_at,fetched_at) DESC` — an
expression no index covers — so SQLite sorts 2.74M rows to return 50, **on the
single mongoose event-loop thread** (`httpd.c:2158` is one `mg_mgr_poll`). One
request freezes the whole server for 35 s. The keyset cursor compares the same
expression, so page 2 costs the same again.

```sql
CREATE INDEX idx_intel_items_pub
  ON intel_items(COALESCE(published_at,fetched_at) DESC, uid ASC);
```

Proof the fix works: `idx_intel_items_simhash_todo` is already on that exact
expression (but partial); constraining a query to its predicate drops the same
sort to **0.001 s**. Also fixes `exportapi.c:490` and `simhash.c:654`.

`ANALYZE` has **never run** — `sqlite_stat1` is absent — so the planner has no
statistics on a 4.76 GB database, and the page cache is the 2 MB default
(0.04% of the DB). `breach_store.c:40` already sets 256 MB on its own
connection; the main handle never got it.

---

## 1. Cross-cutting themes — what several agents found independently

These matter more than any single finding, because independent convergence is
evidence the problem is structural.

### 1.1 Stacked coordinates hide most of the map — unanimous across all three layers

| layer | finding |
|---|---|
| backend | 1,350 camera rows → **562 distinct positions**; 176 on the Tokyo prefecture centroid, 122 on Tokyo Station |
| React client | **no clustering anywhere**; `icon-allow-overlap: true` + `icon-ignore-placement: true` disable collision culling; click handler takes `features[0]` |
| iOS | **no clustering anywhere** — zero hits for `MKClusterAnnotation`/`clusteringIdentifier`; one annotation per feature |

So stacked cameras are not merely invisible — they are **unreachable**: the tap
hits whichever draws last and the other 175 cannot be selected on any client.

Root cause is upstream of all three: a `PREF_CENTROIDS` table + `guess_centroid()`
**copy-pasted verbatim into 10 camera collectors**, plus 47-entry prefecture
tables in 6 more files. Fleet-wide: **1,411 hardcoded coordinate pairs across 35
files, of which only 4 files declare a `geo_precision`**. Every other centroid pin
is indistinguishable downstream from a surveyed coordinate.

### 1.2 `published_at` holds three incompatible formats, and is the primary sort key

Now confirmed at every layer:
- **Backend**: RSS wrote raw RFC-822 (fixed tonight); the 75-source anchor-scrape
  family (`_jp_osint.inc`) sets it **not at all**; `lib/geojson.c:252` accepts only
  strings, so `"timestamp": 1735689600` — the common shape — is silently dropped
  for **~245 collectors**.
- **React**: three sort sites compare raw strings (`' '` < `'T'` < `'W'`), so
  clicking "Last check" produces visibly non-chronological order; `fmtAbs`'s
  fallback is dead code because `new Date('garbage').toLocaleString()` returns the
  *string* `"Invalid Date"` rather than throwing.
- **iOS**: **three date parsers**, and the shared one is the broken one.
  `caseDate` and `TimelineFormat.date` handle SQLite's space-separated
  `datetime('now')`; app-wide `isoToDate` does not — which is why every Alerts
  timestamp renders `—`. 30+ schema columns carry that default.

### 1.3 Honest-empty vs genuine-failure is not expressible

`run()` returns a bare `int`, so "0 rows", "0 = OK" and "n rows" are
indistinguishable. Both directions are live:

- **Quarantine-on-empty**: **45 sources** still `return n > 0 ? 0 : -1`. Exemplar
  `bear_encounters.c:148`, whose own file header argues against exactly this.
- **Silent-failure-as-success**: **VERIFIED** — all 11 MLIT KSJ GeoJSON URLs are
  dead (10 × HTTP 404 with an identical 20,594-byte error page, 1 timeout),
  including every mirror fallback. `lib/mlit_ksj.h` documents that "no mirror
  responded" returns 0 = success, so **six sources report zero forever and no
  row-count sweep can see it.**

That second class is a blind spot in my own audit methodology: verdicts derived
from rows and rc cannot distinguish a dead upstream from a quiet one.

### 1.4 Blocking I/O on the single event loop

**MEASURED: 790× latency amplification** — `/api/health` 0.0006 s → 0.49 s during
one geocode; worst case ~50 s of total freeze. Blocking handlers:

| site | call | worst stall |
|---|---|---|
| `httpd.c:488` → `auth.c:78` | Supabase JWKS fetch **under `g_jwks_lock`**, on every `/api/*` path | ~16 s |
| `httpd.c:1105/1114/1119` | geocoder, 3 providers serially | ~50 s |
| `httpd.c:1956` | camera proxy, per visible tile | 5 s |
| `httpd.c:1240` | tenant webhook POST | 15 s |
| `intelapi.c:300` | the 35 s sort above | 35 s |
| `intelapi.c:542`, `statusapi.c:339` | uncached full-table `GROUP BY` (**MEASURED 38.2 s each**) | 38 s |

`httpd.c:335-339` already documents the correct off-loop pattern and applies it to
two routes. The rest were never converted.

---

## 2. Security — ranked by exploitability × impact

### 2.1 `alertsapi.c` has no authorization check at all — **VERIFIED**

`grep -c "role|role_rank|403" core/alertsapi.c` → **0**. The signature takes
`(db, tid, uid, …)` and **not the role**, so it cannot authorize even in
principle. All six mutating routes are ungated: create, PATCH, DELETE,
mute/unmute, test, mark-read.

A **viewer** can create an alert rule with a permissive predicate and a webhook
channel pointing anywhere — a standing exfiltration channel for the tenant's
entire intel stream — and can delete or mute every detection rule the tenant
relies on. Validation checks only the `http(s)://` prefix: no host allowlist, no
private-IP block.

Internal inconsistency, not a design choice: `savedsearchapi.c:1056` gates the
*equivalent* action behind `role_rank(t->role) < 2 → 403`. **Fix: take a
`tenant_ctx`, gate six branches. Smallest diff of any critical here.**

### 2.2 Breach corpus readable with plain auth

`intelapi.c:211` reroutes `?source=<breach slug>` to `breach_adapter_list()`,
which emits cleartext breached identifiers. `/api/breach/search` gates identical
data behind `opgate_check` with the comment *"Breach data is sensitive, so gate it
like /api/admin."* `breach_items` has no tenant column, so the tenancy nuance
below does not apply. **Fix: move the gate inside `breach_adapter_list()` /
`_reveal_by_uid()` so all four call paths inherit it** — a gate on one of four
doors is not a gate.

### 2.3 Two memory-safety bugs, ASAN-proven

- **`fts.c:9-18` `utf8_next`** reads `s[1..3]` off the lead byte with no bounds
  check, and `fts_has_japanese` then steps **past the NUL**, scanning until it
  chances on a zero byte. ASAN: `heap-buffer-overflow READ … 0 bytes after a
  5-byte region`. Reachable on **every ingest** — `intel.c:97` segments title,
  body, summary, link, author, tags and properties of every item. Trigger is a
  title cut mid-sequence, which the tree manufactures itself
  (`jma_volcano.c:144` `strncpy(summ, sb, 240)`). `entitystore.c:39-56` already
  does this correctly — port it, and delete the `translate.c` duplicate.
- **`o += snprintf(buf+o, sizeof buf - o, …)`** — snprintf returns what it *would*
  have written, so on truncation `o` jumps past the buffer and the next write
  lands at `buf + huge` with a size_t-underflowed limit: a stack write at a
  **computed offset**, which is why ASAN's redzone gets jumped rather than tripped.

  **VERIFIED fleet-wide: 28 accumulator sites, 20 safe, 8 dangerous.** The safe
  ones are fixed-width `%%%02X` in a bounded loop, or pre-gated. The dangerous
  ones write a variable-length `%s` from upstream JSON:

  ```
  p2pquake_jma.c    :79 :80 :87     ← the ASAN-proven one
  unified_flights.c :46 :48 :49
  certstream_jp.c   :131
  wolfx_eew.c       :80
  ```
  Three of those files are new — the agent named only `p2pquake_jma.c`; running
  the grep it recommended found the rest.

### 2.4 Other confirmed

- **HTTP method is not enforced** — **VERIFIED**: `/api/data/cameras/trigger`
  documents itself `POST` and has no method guard; a **GET** fanned out 16
  scrapers, and a GET on `/api/intel/sources/:id/run` wrote 474 rows. Any
  link-prefetching client mutates state.
- **Export filters silently dropped → full-dataset egress** — `qs[2048]`
  truncates while `format` is read from the *untruncated* query, so you get a
  correctly-named CSV containing everything. Proven: `&source=NOSUCHSOURCE` → 0
  rows; add 2,100 bytes of padding → 474 rows. Enterprise row cap is `-1`.
  **Nine more buffers share the pattern.**
- **Break-glass admin login** is unauthenticated, unthrottled (there is **no rate
  limiting anywhere in the server**), and **non-functional** — it mints a token
  signed with `BREAK_GLASS_JWT_SECRET` that `auth_check()` has no path to verify.
  It would be discovered broken *during* the emergency.
- **SSRF**: camera proxy accepts any `intel_items` uid with no source or tenant
  filter; `httpclient.c` has no protocol pin, no private-IP block, and **no
  response-size ceiling** (a 1 MB gzip expanding to many GB OOMs the process).
- **Secrets**: `content_change.c:1217` derives `ref_key` from the **raw** URL, so
  `api_key`/`token` query params are stored in cleartext and reachable via the
  API — the adjacent evidence path redacts correctly via `redact_url`.
- **Tenancy is latent, not live** — the agent corrected its own severity after
  checking all three `intel_sink_make()` call sites: every row today is
  `'legacy'`, so there is no tenant-private intel to leak *yet*. It becomes a
  full corpus dump the moment per-tenant collection ships. Separately,
  `pipeline.c:174` writes every user's search into the shared bucket — which the
  tenant predicate would **not** fix.

### 2.5 Verified clean — where not to spend effort

No SQL injection anywhere (all bound; `dbexplorerapi` allowlists tables and
validates `orderBy` against real `PRAGMA table_info`). No format-string bugs. No
command injection (`popen` behind a path allowlist; `ffmpeg` uses `execvp`).
Upload path traversal closed. Client XSS-free across 54 files. iOS Keychain usage
correct. Git history clean of secrets. All three vendored C libraries current
(sqlite 3.49.2, cJSON 1.7.18, mongoose 7.21) with no applicable CVEs. Tenant
isolation itself is sound — an agent went hunting for cross-tenant IDOR
specifically and could not break `tenant_resolve()`.

---

## 3. Correctness

### 3.1 Three collector paths share the HTTP connection across threads

`scheduler.c:137-141` documents why this is unsafe and fixes it for the cron
loop. `httpd.c:741`, `:2064` and `:500` still pass `g_db` into detached threads
that run concurrently with the mongoose loop. A `BEGIN` inside `emit()` fails
(unchecked), the upsert lands inside an HTTP handler's transaction, and
`emit()`'s `COMMIT` commits someone else's half-written row — or its `ROLLBACK`
discards it. `sqlite3_changes()` then reports the other thread's statement, so
alert and simhash hooks fire on unrelated requests.

### 3.2 The scheduler is one serial loop, 2.4× oversubscribed

1,590 scheduled sources demand 3,119 runs/hour = a 1.15 s serial budget each;
measured mean is 8.6 s. **The fleet needs 2.4 core-hours per hour on one
thread.** `next[i]` is computed from the pass-start time, so once a pass exceeds
an interval the source re-runs back-to-back with zero idle. Boot stagger `i*3`
puts source 1,998's first run **99.9 minutes** after start. Every declared
`update_interval_sec` is fiction.

**MEASURED**: 216 runs (10.9%) consume 88.6% of the sweep wall clock; 740 runs /
2.67 h produce nothing.

### 3.3 `sources` operational columns are never written — **VERIFIED**

Only three `UPDATE sources` statements exist in the tree: `capture_evidence`,
`quarantined_until`, `schedule_mode`. Nothing writes `status`, `last_check`,
`last_success`, `records_count`, `error_message`.

Consequence, arithmetically:
```
reliability = 0.40*success + 0.30*freshness + 0.20*(1-anomaly) + 0.10*volume
freshness_of() → 0.0 when last_success is NULL   (source_trust.c:51)
ceiling 0.70 → grade_of(0.70) = 'C'              (0.90='A', 0.80='B')
```
**'C' is the best grade any source in the fleet can ever earn.** The ops dashboard
shows `pending`/`null` for all 1,998 sources permanently, and the LLM triage
prompt reasons on blank context. ~15 lines beside the existing `fetch_log_write`
unlocks all of it.

### 3.4 The registry does not reach the map — **VERIFIED**

`source_registry.gen.c` holds **412 curated rows; 92 describe collectors that no
longer exist**, producing **58 phantom layer ids** that `/api/layers` publishes
with zero collectors behind them — `haneda-flights`, `narita-flights`,
`jpx-quotes`, `mext-schools`, `crowd-density`, `elevation`, … Each answers 200
with an empty FeatureCollection instead of 404.

Worse, `src_meta_at()` enumerates **only** the curated table, so **1,678
registered sources are invisible to `/api/layers`**, and a new source physically
cannot create a map layer without hand-editing the generated file. **21 scheduled
sources emit geometry and have no layer from either source** — their pins are
collected daily and can never render.

### 3.5 Data-integrity bugs in the ingest path

- **147 of 222 GeoJSON sources key rows by a content hash**; **23 also carry a
  changing measurement**, so upsert becomes unbounded insert (`agoop-flow`,
  `docomo-population`, `marinetraffic-jp`, `unified-flights`, …).
  `overpass_collect` computes a stable `type/id` for dedupe and then **throws it
  away** instead of putting it in properties — one line would fix ~95 sources.
- **`entitystore.c:119`** is the unfixed twin of a bug `intel.c:109` documents as
  fixed: `last_insert_rowid()` used unconditionally after a discarded step, so a
  failed FTS insert points entity X at entity Y's rowid and the next write
  **evicts Y from search**.
- **Duplicate `intel_items_fts` rows fan out through three unguarded joins**
  (`intelapi.c:291`, `exportapi.c:492`, `alert_eval.c:1144`) — duplicates consume
  the `LIMIT`, so other items silently fall off the page.
  `alert_eval.c:590` already uses the immune pattern.
- **`es_upsert_entity`** is check-then-act with no transaction: a race returns an
  entity id that does not exist, and `write_entity_fts` has already made it
  searchable — a hit that resolves to 404.

### 3.6 `lib/` — one bug here is a fleet bug

- **`csv.c`** never strips the UTF-8 BOM, so the first column vanishes for 7 CSV
  collectors — `police_incidents.c` loses every latitude. `gtfs_jp.c:174` strips
  it privately; the shared helper doesn't.
- **`csv.c:87`** opens `iconv("UTF-8","SHIFT_JIS")`, which is strict JIS X 0208
  and rejects `①`, `㈱`, `～` — ubiquitous in NPA CSVs. On failure it stores the
  **raw Shift_JIS bytes**. `CP932` transcodes them correctly.
- **`htmlparse.c:100`** — `html_attr` has no left word boundary, so `href` matches
  inside `data-xhref` and `src` inside `data-src`: lazy-loading pages hand back
  the placeholder image forever. No entity decoding anywhere, while the repo
  contains **four** private decoders.
- **`bigfile.c:23`** — one embedded NUL destroys **two** lines and the skip count
  is not exposed, so a UTF-16 dump silently loses nearly every record.
- **`zipread.c:174`** — the deflate-bomb hardening was applied to `inflate_raw`
  and **not** to `zip_first_entry`, a verbatim copy with the ceiling removed.
- **`unified.c:136`** — merging 3+ upstreams keeps only two sources; the rest
  vanish, which is the entire point of a `unified-*` collector.

---

## 4. Clients

### React (54 files)

- **`useRef(loadPersistedCache())`** — `useRef(x)` evaluates `x` on *every*
  render, so **every keystroke in the map search box re-parses up to 5 MB × N
  cached layers** from sessionStorage. Multi-second freeze per character.
- **Two visible controls are wired to nothing**: the opacity slider (baked into
  `paint` at add time; `setPaintProperty` never called for it) and the temporal
  window (`temporalWindow` is written and read only to render its own `<select>`;
  no `setFilter` exists).
- **Switching basemap permanently reverts every pin to a plain dot** —
  `registerLayerIcons` is `async` and not awaited, so `map.hasImage()` is false
  for every layer in the same tick.
- **Background refreshes never reach the map** — the re-add condition is
  `wasData !== currentDataLength`, and stale-while-revalidate is exactly the case
  where the count is identical and the values changed. Aircraft, ships, river and
  tide levels silently freeze.
- **No `ErrorBoundary` anywhere** — six known throw sites turn one malformed API
  response into a white screen.
- **Every layer is fetched whole**, no bbox, no zoom gating, and "All On" fires
  ~150 unbounded concurrent requests.
- Clicking an aircraft below 2,000 ft opens the wrong popup (`-b-Infinity` defeats
  the `\d+` strip regex).

### iOS (146 files)

Genuinely well-built — zero `as!`/`try!`, modern navigation, 21
`ContentUnavailableView`, disciplined 44pt targets, strong Japanese handling, and
`SWIFT_DEFAULT_ACTOR_ISOLATION = MainActor` makes the classic off-main mutation
bug structurally impossible. Defects are narrow and severe:

- **Four reachable crashes**: force-unwrapped `URL(string: base + …)!` on the
  user-editable backend URL (hit on *every* search); force-unwrapped YouTube
  embed URL built from an unvalidated server string; **`TimeSliderView.swift:302`
  builds `lowerBound ... upperBound` from two independently-published optionals**
  — when the newest data is older than 7 days, routine between ingests, the
  bounds cross and the map's most prominent control traps; and the
  `enumerated()/id:\.offset` + `.onDelete` anti-pattern in `AlertEditor`.
- **Timeline pagination dies permanently** — `fetch` bumps `loadToken` on entry
  but clears `loadingMore` *behind* the token guard, so any reload landing
  mid-`loadMore` leaves the flag stuck forever. Scroll while scrubbing and it is
  dead for the life of the screen.
- **The `mapBarSurface` preference is violated 9 times on the bars themselves** —
  **VERIFIED**. The token's own comment explains that flat frosted material
  *cannot* match the tab bar's Liquid Glass; 8 controls sitting on those bars use
  `.thinMaterial`, the exact material it rules out.
- Whole-envelope decoding: one null in `CaseSummary` or `EntityBreach.data_classes`
  takes down an entire page. The file **documents this having happened in
  production** and applied the lesson to one struct but not the two originals.
- Camera grid instantiates unbounded `WKWebView`s and `AVPlayer`s with **zero
  `dismantle*` hooks** — the classic "only the first four videos play" cliff.

---

## 5. Performance — measured

| finding | measurement |
|---|---|
| `/api/intel/items` sort | **35.1 s** → 0.002 s with one index (**VERIFIED**) |
| `emit()` per-row transaction | **725 µs/row → 27 µs/row, 26.9×** (MEASURED benchmark; 96% is `BEGIN`/`COMMIT`, 4% the per-row prepare) |
| `/api/intel/sources` + `/api/status` aggregates | **38.2 s each**, uncached, full scan, on the event loop |
| SQLite page cache | 2 MB default on a **4.76 GB** DB; `ANALYZE` never run |
| `intel_items` indices | 14 indices / **1,300 MB** — two keyed on `tenant_id` which is `'legacy'` on 100% of rows; two "partial" indices matching 100% and 99.9% |
| FTS duplicate storage | **577 MB**; the external-content pattern is already used by `breach_fts` |
| simhash | 2 MeCab passes per ingested row; total yield across the corpus = **369 clusters** |
| Overpass tiled dedupe | tile 1 is a strict **superset** of tile 0, so 100% of tile 0 is duplicate work; dedupe silently stops past 200k keys (parking-facilities is 474k) |
| retention | **nothing ever deletes an `intel_items` row** |

---

## 6. New sources — live-verified

The proposals were probed with `curl` and byte counts; 13 candidates were
**rejected with evidence** (403 Akamai, Cloudflare interstitial, `401 token
required`, SPA shell, and one whose content is frozen at 2024).

| # | source | key-free | real coords | effort |
|---|---|---|---|---|
| 1 | **`jma-amedas`** — 1,286 stations, temp/wind/precip/snow/visibility, 10-min | yes | **1,286 distinct real points** | 2–3 h |
| 2 | `jma-xml-structured` — landslide + river-flood + heat-stroke warnings (the feed is already polled and the payload thrown away) | yes | area-code level | 1 day |
| 3 | `sentinel2-earthsearch-jp` — replaces the permanently-gated `sentinel-japan` | yes | scene-footprint polygons | 2 h |
| 4 | **`jp-chome-gazetteer`** — 277,657 chome-level coordinates, CC BY 4.0 | yes | **it is the geocoder** | 1 day |
| 5 | `tokyo-opendata-geo` — 9,647 datasets, 9,644 CC BY 4.0 | yes | yes, if gated on a 緯度/経度 header sniff | 1 day |
| 6 | `osm-notes-jp` — ground-truth "this changed" reports | yes | exact per note | 2 h |
| 7 | `nga-modu` — 648 offshore rigs incl. 4 on the East China Sea median line | yes | yes | 3 h |

**#1 and #4 are the ones that matter**: 1,286 real coordinates for 2–3 hours of
work, and the gazetteer that actually fixes the centroid problem rather than
adding another layer on top of it. Do not add more news — there are already ~400
`gnews-*` plus ~250 outlets.

Incidental but important: `jma-weather` fetches **one hardcoded URL** (Tokyo)
despite its name, and `jma-warnings` pins to a hardcoded 47-prefecture table when
JMA publishes 142 forecast offices — the centroid defect again, in a new family.

---

## 7. What to do, in order

**Today — hours, high certainty:**
1. `CREATE INDEX idx_intel_items_pub …` + `cache_size` / `mmap_size` / `ANALYZE`
   (6 lines of SQL; 35 s → 2 ms on the flagship endpoint)
2. `alertsapi.c` — pass `tenant_ctx`, gate six routes (pattern exists in
   `savedsearchapi.c:1056`)
3. Move the breach gate inside `breach_adapter_list()` / `_reveal_by_uid()`
4. Bound `utf8_next` (port `entitystore.c:39-56`); clamp the 8 `snprintf`
   accumulators
5. Write the `sources` operational columns (~15 lines; unlocks trust grading and
   the ops dashboard)
6. Enforce HTTP method on the mutating routes; reject over-long query strings
   with 414 rather than truncating

**This week:**
7. Cache statements + batch the transaction in `emit()` (26.9× ingest)
8. Own DB connection per off-loop thread (`db_attach`, as `sched_thread` does)
9. Move JWKS refresh, geocoding and the camera proxy off the event loop
10. Cluster/spiderfy stacked points on **both** clients; make opacity and the
    time window actually drive the map
11. iOS: one date parser, generation-guard the WebSocket, the four crash fixes

**Structural:**
12. Worker-pool scheduler with a per-source budget and a real cancel
13. Make `src_meta_at()` enumerate the live registry; delete the 92 orphan rows;
    mark retired sources instead of leaving them in `sources` forever
14. `lib/text.c` — one UTF-8-safe truncate, one entity decoder, one charset sniff,
    one `jo_time_normalize()`. Retires whole defect classes across ~586 collectors
15. Four `source_def` fields — `accepts` (pivot kind), `record_kind` (closed
    vocabulary), `geo_precision` — each retires a class
16. `make lint-sources` + CI: duplicate ids, orphan `.o`, orphan curated rows,
    missing metadata, geometry-without-layer, the `? 0 : -1` shape. Nine of the
    twelve architecture findings are mechanically checkable and none is checked.

---

## 8. Method notes and limits

- Two agents delivered only an addendum; I retrieved the full reports via
  `SendMessage` rather than reporting partial findings.
- One agent **corrected its own severity** mid-report after checking all three
  `intel_sink_make()` call sites — the tenancy exposure is latent, not live. That
  correction only happened because it checked rather than assumed.
- I corrected **my own** earlier Overpass fix: it covered `collect_fetch` and
  missed `tiled_fetch` (12 tiles × 4 endpoints, worst case 48–96 min in one
  scheduler slot). Now fixed and building clean.
- I also corrected an over-count of my own: "28 unguarded `snprintf`
  accumulators" was wrong; 20 are safe, 8 are not.
- The iOS report is **static analysis only** — no macOS toolchain here, so the
  four crash claims are reachability arguments, not observed traps, and the agent
  listed six things it could not verify without a build.
- The contract harness (`tests/contract/run.sh`) now runs but its fixtures are
  from a 269-source era and one captured an error response; it cannot validate
  anything until re-captured on a host with Node.
