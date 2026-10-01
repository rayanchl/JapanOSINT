# JapanOSINT — Full-Tree Audit Report

*Synthesis of five independent adversarial reviews. Branch `feat/osint-batch12-c-port`, measured 2026-08-15.*

---

## 1. State of the tree

The engine is better than its census. `native/source.h`'s one-struct ABI is genuinely honoured — every registration path converges on `registry_add()`, there is no second service registry, `core/intel.c` is a real single sink that stores every field verbatim with no cap, and `core/pipeline.c` is a textbook rule-2 consumer (stores everything, hands the LLM a labelled `records_shown`/`record_count`/`prompt_truncated` view). Rule 1 is held across the overwhelming majority of ~9,400 sources: `real_data:true` appears nowhere in code, the 19 collectors the old reality report flagged as "names, not data" have been rewritten to honest-empty, and ~25 files carry an explicit "the curated SEED_* fallback is intentionally not ported". Against that, three things are not solid. **(a) The guarantees cover 6% of the fleet.** `lib/hpengine.c` is the only code path with pagination, detail-hop, full-field flatten and in-band truncation notices — 601 of ~9,436 registered sources, 29 of 1,204 files. The `_verified_macros.inc` family that backs ~6,500 sources (67% of the registry) has *no* pagination concept and *no* truncation-notice path at all; 2,081 of its URLs hard-code a page size and never request page 2. **(b) The gates are decorative.** `make audit-sources` reports 147 findings across 91 files but passes `--strict` only over `collectors/sources/hp*_*.c`, so it exits 0; CI never runs it at all (4 steps: build, selftest, unit, lint-sources). `lint_sources.py` structurally cannot see `HP_REGISTER_TABLE`, so its dup-id check is blind to 601 sources — and one live duplicate (`UK_CH_FILING_HISTORY`) is already falling through that hole with CI green. CLAUDE.md's "zero audit findings" and `docs/SOURCE_EXHAUSTIVENESS.md:98`'s "685 scanned files" are both false (147 findings, 1,213 files scanned); `grep -rn exhaustive-ok` returns 142, not 145. **(c) The tree has no taxonomy, only chronology.** 383 filename prefixes, 289 of them owning exactly one file; a `.collector` field whose 86 values conflate topic, geography and dispatch mode; 416 files still declaring a directory deleted in June; 308 endpoints fetched by more than one collector (363 redundant registrations across 242 files), none of them deduped because `intel.c:206` keys on `source_id|remote_key`. And the fleet's own runtime database says 443 sources have produced zero records in 10+ runs — 249 of which report `status="ok"` every single time.

**Count reconciliation (read this before quoting a number).** The batch-14 promotion is *half-landed in the worktree*: 20 `csrc14_*.c` deleted (unstaged), 17–20 `vsrc14_*.c` untracked, `native/bin/japanosint` stale. Consequently:

| Measurement | Pre-promotion (HEAD) | Post-promotion (worktree) |
|---|---|---|
| `lint_sources.py --count` | 9,096 (1,115 direct + 7,981 macro) | **8,835** (1,115 + 7,720) |
| `HP_REGISTER_TABLE` rows (invisible to lint) | 601 | 601 |
| **True registered fleet** | ~9,697 | **~9,436** |

`./bin/japanosint --list-sources | wc -l` printed 9697 while lint printed 9096 at the same moment. The 601 gap is exact and structural, not drift.

---

## 2. Confirmed findings

### CRITICAL

**C1 — `SOCIAL_EMAIL` fabricates a confirmed Twitter account for every email queried.**
`native/collectors/sources/social_search.c:45`
Every `SOCIAL_EMAIL` pivot on any email persists an `intel_items` row asserting, at confidence 90 with `detection_method:"direct"`, that the address has a Twitter account — derived from a GET that never contains the address. GitHub (404) and Pinterest (403) share the defect and are saved only by their non-200 status. This is the exact thing rule 1 forbids: a finding that was never fetched, stored as fact.
**Fix:** put the entity token in the GET URL (`…email_available.json?email={}`) or drive the GET branch from `data_template`, and anchor indicators on the value (`"taken":true`/`false`) not the key. Until then, delete the three GET rows from `holehe_sites[]`.

**C2 — The camera layer emits a hand-transcribed catalogue as discoveries, with zero network I/O, and then upgrades guesses to `geo_precision:"exact"`.**
`native/collectors/sources/cam_curated_jp.c` (194 rows) · `native/collectors/sources/camera_geocode_pod.c`
194 hand-typed camera records are emitted as camera discoveries without a single request; the URLs were verified live and return 404. `camera_geocode_pod.c` then stamps name-geocoder guesses as `geo_precision:"exact"` at distances up to 150 km from the anchor — the same defect `docs/codebase-audit-2026-08-07.md:304` flagged a week earlier for prefecture-centroid rows. Both are invisible to the operator because the client never renders provenance fields.
**Fix:** delete the 194-row table or convert it to a URL manifest that is actually fetched; cap `camera_geocode_pod` at a real precision ladder (`exact` only from a coordinate-returning geocoder response) and emit the geocoder confidence in-band.

### HIGH

**H1 — `strtok()`'s process-global cursor is raced by 8 scheduler workers plus the HTTP thread.**
`native/lib/jsonlist.c:246` (and the same idiom in `hp_pick`)
`core/scheduler.c` runs 8 workers concurrently with mongoose. Thread A's `strtok(bufA,".")` then B's `strtok(bufB,".")` overwrites the global; A's `strtok(NULL,".")` resumes inside B's stack frame — an out-of-bounds read of another thread's stack returning a garbage key. `cJSON_GetObjectItem(cur, <garbage>)` → NULL → `jsonlist_find_array` NULL → `jsonlist_emit` returns 0 (`:383`) and **142 sources report an honest-looking empty result for a fetch that succeeded**, non-deterministically, with nothing logged. In `hp_pick` it silently selects the wrong title/id key.
**Fix:** mechanical sweep to `strtok_r` with a per-call `char *save`, plus a grep guard in `make audit-sources` rejecting bare `strtok(` under `collectors/`, `lib/`, `core/`.

**H2 — The operator-authorization check tokenizes with `strtok` and is corruptible by any concurrent collector.**
`native/core/operatorgate.c:12`
A collector thread's `strtok(body,"\n")` landing between this function's two calls re-points the global cursor into the collector's heap body. `csv_has` then either terminates early — non-deterministically denying a legitimate platform operator — or walks attacker-influenced upstream response bytes and compares them against the operator's email/id. It also `return`s from inside the loop, leaving the global cursor dangling into an abandoned stack frame. *(Filed medium by the reviewer; escalated here because it is an authorization decision depending on unsynchronized global state mutated by remote response parsing.)*
**Fix:** `strtok_r` here first, independently of the H1 sweep.

**H3 — API keys saved through `/api/keys` never reach any collector, while `/api/status` reports them set.**
`native/core/statusapi.c:38` · keys land in `data/api-keys.json`, never applied to the process environment
An operator adds a key in the iOS tab; `/api/keys` and `/api/status` both report `set=true`; every gated source runs keyless forever, returns honest-empty, and `run_status()` (`core/scheduler.c:71`) classifies that as **"ok"**. The credentialed half of the fleet is dark and the dashboard is green.
**Fix:** apply the overlay to the environment at boot next to `load_dotenv()` in `main.c`, or resolve credentials through one overlay-then-env helper — and have gated collectors emit an explicit `needs-credential` record instead of an empty result.

**H4 — Six collector join helpers never increment their element counter; the field is permanently dropped or glued together.**
`native/collectors/sources/academic_world.c:40` and five siblings · `native/collectors/sources/domains_world.c:61,78`
In five of six the function unconditionally returns NULL, so **Crossref authors, Semantic Scholar authors, OpenAlex authors, NIH principal investigators, DBLP authors and Polish KRS board members are absent from every emitted record** — bytes fetched and discarded at a seam, with no truncation notice because the code doesn't know it lost anything. In `domains_world.c` the buffer *is* used but the `if (n)` separator never fires, so Cert Spotter SANs persist concatenated with no delimiter (`example.comwww.example.commail.example.com`) into both the field and the item title.
**Fix:** six one-line `n++` additions matching `reg_ie_cro.c:60`, plus a `tests/unit` case feeding each helper a two-element array — this family will regress otherwise.

**H5 — VERIFIED BUG: hpengine paginates in JSON mode only; HTML and CSV silently stop after page 1 with no notice.**
`native/lib/hpengine.c:828` (zeroes `st.page_records`), `:676` (only `hp_run_json` sets it), `:856` (`if (st.page_records <= 0) break;`)
Because `st.truncated` is never set on those paths, **no `collector-truncation-notice` is emitted** — the failure is invisible in the data, which is precisely what rule 2 forbids. Compiled and run in WSL against the real `lib/hpengine.c`: HTML paged row = 1 HTTP call / 2 records / no notice; CSV paged row = 1 call / 2 records; JSON paged row = 3 calls / 3 records. Shipped rows affected: `hp2_eu_institutions.c:199` (`EU_EUIPO_TRADEMARKS`, `.page_param="page"`) and `hp2_northam_ca_mx.c:102` (`CA_CIPO_TRADEMARKS`, `.page_param="start"`, `.page_size=50`). 291 of 601 rows are `HP_HTML`, so paging is structurally unavailable to half the engine fleet. `tests/hpengine_test.c` has no HTML/CSV pagination test — exactly the gap.
**Fix:** set `st.page_records` in `hp_run_csv`/`hp_run_html`; add the two missing test cases.

**H6 — A live duplicate source id exists across the two registration mechanisms, and no checker can see it.**
`native/collectors/sources/corp_lifecycle.c:512` (REGISTER_SOURCE) vs `native/collectors/sources/hp_uk_deep.c:61` (HP table)
`--list-sources` prints `UK_CH_FILING_HISTORY` twice; startup prints `[registry] DUPLICATE id … second definition is unreachable`. Which one loses depends on constructor order. `make lint-sources` says `dup-id 0 at baseline` because `lint_sources.py`'s extractor keys on `_REG_RE = REGISTER_SOURCE\s*\(` (`:360`) and `HP_REGISTER_TABLE` never emits that token — verified: `L.registrations('hp_uk_deep.c')` returns **0** for a file with 22 live rows. `SOURCE_AUTHORING_CONTRACT.md` R5 says "a clean run must print none"; that gate is violated today. `registry_duplicate_count()` (`native/registry.c:61`), the one function that could turn this into a build failure, has **zero callers in the tree**.
**Fix:** teach `lint_sources.py` to parse `hp_source` tables; call `registry_duplicate_count()` from `--selftest` and fail non-zero; delete the hand-rolled `corp_lifecycle.c` definition in favour of the hp row (which pages: `.page_param="start_index"`, `.page_size=50`).

**H7 — `osint_canon()` upper-cases before `registry_get()`, so 1,066 lower-case-id services the dispatcher itself advertises can never be resolved.**
`native/core/osint_dispatch.c:31` (`osint_lookup()` = `registry_get(canon)`)
The dispatcher injects the `collector=="osint"` subset into the LLM's JSON-schema enum, so the routing model is told these services exist; every pivot to one of them fails to resolve. The seven-source finding below (`geoeo_elevation_points.c` et al.) is one visible corollary.
**Fix:** case-insensitive lookup in `registry_get`/`osint_canon` (preferred — it also unblocks the seven), or rename the affected ids to UPPER_SNAKE.

**H8 — `gen_verified_sources.py` never reads the manifest's `verified` column; a proof-of-fetch header is one CLI flag away from being a lie.**
`native/collectors/gen_verified_sources.py:155-211` (columns read), `:242` vs `:255` (header selection)
Whether a generated file asserts *"Every endpoint in this file returned 2xx and parsed to at least one record at generation time"* or the honest UNVERIFIED block is decided **solely by whether the operator typed `--unverified`**. Feeding it `docs/candidate-sources-batch14.tsv` (every row `verified=no`) without the flag would emit 1,001 collectors carrying a false proof-of-fetch assertion, and no lint, audit or CI step would flag it. The batch-14 numbers are the evidence for why this matters: 1,001 candidates → **740 PASS, 261 rejected** (237 HTTP_ERR, 11 NET_ERR, 8 TOO_BIG, 4 UNPARSEABLE, 1 TIMEOUT) — a 26% dead rate on rows the candidate generator's own docstring calls "documented platform API contracts". Those 261 were registered, schedulable and dispatchable the whole time they sat in the tree.
Related, confirmed: the freshly-generated `vsrc14_*.c` headers hardcode *"see docs/verified-sources-manifest.tsv for the recorded proof"* (`:258`) while their rows live in `docs/verified-sources-batch14.tsv` — `grep -c us-arcgishub-addresses docs/verified-sources-manifest.tsv` → **0**, batch14 → **1**. The audit trail points at the wrong file.
**Fix:** read the `verified` column and refuse to emit the proof header unless it is `yes`; parameterize the manifest path into the header instead of hardcoding a batch literal.

### MEDIUM

**M1 — The bulk of the registry has no pagination and no truncation-notice path at all.**
`native/collectors/sources/_verified_macros.inc` (VRSS/VJSON/VGEO/VCSV) over `lib/jsonlist.c`, `rss_atom.c`, `geojson.c`, `csv.c`
6,517 sources across 144 generated files; each is exactly one GET of one fixed URL. **2,081 of those URLs hard-code a page size** (`limit=100`, `rows=100`, `per_page=100`, `pageSize=100`) and never fetch page 2, and the audit's single-page regex only looks for `page=1`/`offset=0`, so it does not even flag them. Verified live: one of these fetches `api.dane.gov.pl/1.4/datasets?page=1&per_page=100`, whose response carries `meta.count: 26536` and `links.next`, emits 100 records and `cJSON_Delete()`s the rest — a 99.6% silent discard. Only **three code sites in the entire tree** emit `collector-truncation-notice`: `lib/hpengine.c:921`, `_jp_osint.inc:160` (`jo_emit_anchors`) and `diet_records.c`.
**Fix:** this is the single highest-leverage change in the report — port the page-walk + notice machinery *down* into `jsonlist_emit`/`_verified_macros.inc` (a `next_path`/`page_param` field on the macro), which reaches ~6,500 sources for one edit. Migrating collectors onto hpengine does **not** substitute: `hp_run()` returns 0 when `ctx->entity` is empty (`hpengine.c:768`), all 601 rows have interval 0, it never sets `geometry_geojson` and hard-codes `.layer = NULL` (`:956`) — no scheduled feed or map layer can move onto it as-is.

**M2 — `od_pl_dane.c` discards 99.92% of the catalogue and stores the detail URL instead of following it.**
`native/collectors/sources/od_pl_dane.c:14`
20 of 26,536 datasets, from a keyless unthrottled national catalogue, with the count sitting in the same parsed document. `resources_url` — the JSON:API sub-collection holding the actual files per dataset, i.e. the records the source exists to provide — is serialized into properties as a string rather than fetched. Only 3 of 55 `od_*.c` files contain any page-walking.
**Fix:** `per_page=100` + follow `links.next` (`od_shared.c` already has the helper), fetch `resources_url` per dataset, stamp `meta.count` as `records_available`.

**M3 — Collectors whose own comments state the discard ratio, with no marker and no notice.**
`native/collectors/sources/soc_packages_feeds.c:255` and siblings
The author wrote down the exact ratio being discarded (300 of 25,000; 200 of 1,000) in a comment — a log line by another name. The full array is already parsed in memory; emitting the rest costs **zero extra requests**. As shipped, a query for a homebrew formula ranked 400th returns `not_found`, indistinguishable from one that does not exist.
**Fix:** delete the breaks. `total_count` is already parsed and can populate `records_available` directly.

**M4 — hpengine reports a dead endpoint, a 401/403/404 and a missing credential identically as an empty success.**
`native/lib/hpengine.c:874`
The engine has an in-band notice for truncation (`:920`) but none for "this endpoint is gone" or "this needs a credential". A permanently dead row and a genuinely empty search look the same to the analyst and to `source_trust`, so the ~10% of hp rows that are hard-dead sit in the catalogue indefinitely. Rule 1 requires failure to degrade to an explicit error/not_found/needs-credential.
**Fix:** emit `collector-endpoint-error` / `collector-needs-credential` records carrying status code, URL and env-var name — same shape as the truncation notice, so dead rows can be swept mechanically.

**M5 — hpengine's truncation accounting lives in plain globals shared by every worker.**
`native/lib/hpengine.c:373` (`g_flat_drops`, `g_flat_trunc`) vs `core/pipeline.c:318` (up to 16 concurrent dispatches)
Two workers interleave the reset and the increments: a complete record gets stamped `_fields_dropped` from another row's oversized record, and a genuinely truncated record can ship stamped 0. It is UB and a TSan report, but the worse consequence is that **a truncated record ships as if it were whole** — the very stamp rule 2 depends on. The comment at `:371-372` ("Reset by hp_flatten() at each record") assumes a single-threaded engine that no longer exists; `lib/jsonlist.c:207` already uses `static _Thread_local` for its scratch, so the concurrency is known in the same directory.
**Fix:** make both `__thread`, or thread them through the `int *drops, int *trunc` out-params `hp_flatten_c` already accepts.

**M6 — Two collectors fetch a URL, `free()` the body unread, and emit an empty array by construction.**
`native/collectors/sources/dam_water_level.c:18` · drone-nofly
A request is spent and the whole response is thrown away at the collector, with no `exhaustive-ok` exemption, and the run is logged as a success with zero rows — indistinguishable in `fetch_log`, `/api/status` and anomaly detection from a source that legitimately had nothing. 144 requests/day for guaranteed nothing.
**Fix:** parse what the endpoint returns, or delete both `source_def`s.

**M7 — 308 endpoints are fetched by more than one collector; every hit is stored N times.**
242 files, 363 redundant registrations, 316 after host/scheme normalisation (extracted with the Makefile's own `grep -hoE 'https?://[^" ]+'` trick: 13,740 URL-bearing lines, 10,434 distinct URLs, 9,922 real endpoints)
`core/intel.c:206` keys every row as `uid = "<source_id>|<remote_key>"`, so nothing dedupes: the same upstream record lands N times in `intel_items` and N times in the FTS mirror. Confirmed exact-URL pairs: `cyi_atlas_anchors.c` (`atlas-anchors`) vs `tsp_ripe_atlas_anchors.c:158` (`ripe-atlas-anchors`) — byte-identical RIPE Atlas URL, both scheduled; `geoeo_ioc_sealevel.c` vs `mar_ioc_sea_level_stations.c` (ids differ by one hyphen); `geoeo_ndbc_buoys.c` vs `eni_ndbc_buoys.c:27`. Same pattern for Safecast, NOAA CO-OPS, UK flood and USGS water across `eni_`/`geoeo_`/`mar_`. Four `run()` bodies are **byte-identical across files, strings included**; 13 more are structurally identical; three are whole-file copies where only `.id`/`.name`/`.description` differ — including `REVERSE_GEOCODING`, which never calls a reverse endpoint, and `DOMAIN_HISTORY`, which is `domain_age.c` with one summary string changed. Ruled out as false positives: the 11 `mar_emodnet_*` files (same WFS base, different `typeName`), the `tsp_fcc_{am,fm,tv}` trio, and all `csrc14_*` (zero URL intersection with the tree).
**Fix:** add a URL-normalizing duplicate check to `lint_sources.py` (baselined, ratcheting down); merge per §3(c).

**M8 — 443 registered sources have never produced a record; 249 of them report `ok` on every run.**
Cross-check of the live runtime DB `data/japanmap.db` (71,453 `fetch_log` rows, 2026-07-18 → 2026-08-15) against 3,881 distinct collector hostnames
53 hostnames have **no address record under two independent resolvers**, killing ~40 named sources outright. 198 sources have never returned `ok` in 5+ runs. 443 have produced zero records in 10+ runs — and 249 of those report `status="ok"` every time, so they are invisible to `/api/status` and to anomaly detection. Nothing in the engine distinguishes a verified source from an unverified one at runtime: `source_def` (`native/source.h:78-106`) has no verified/provenance/last-probed field, `db_seed_sources` (`core/db.c:106`) seeds every id identically, `scheduler_loop` (`core/scheduler.c:405-411`) filters only on interval/search-only/quarantine.
**Fix:** a `last_ok_at` / `records_ever` column on `sources` written by `scheduler_run_source`, and a scheduler predicate that demotes (not deletes) a source with 10+ record-free runs into a low-cadence probe lane, emitting the demotion as data.

**M9 — Documented claims contradict measured reality in three places.**
`CLAUDE.md` ("The tree is at zero audit findings", "145 exhaustive-ok markers") · `docs/SOURCE_EXHAUSTIVENESS.md:98` ("zero findings across all 685 scanned files") · `native/collectors/SOURCE_REALITY_REPORT.md`
Actual: **1,213 files scanned, 91 with findings, 147 findings** (first-only 52, single-page 34, record-cap 32, loop-break 27, limit-one 1, dedupe-ring 1); `grep -rn exhaustive-ok` → **142**. Only `strict set (hp*_*.c): 0` is true. `SOURCE_REALITY_REPORT.md`, which CLAUDE.md cites as "full audit of how each collector behaves today", audits 61 collectors in `native/collectors/osint/sources/` — a directory that has not existed since commit `45affc9` (2026-06-21) — out of ~9,400 live sources. Two `exhaustive-ok` markers are demonstrably false: `theharvester.c` claims its cap is "logged" in a file containing zero `fprintf` calls, and the eleven `world_reg_*`/`reg_*` "runaway guard, logged" markers point at a log line reporting the *full* registry-table size even when the loop broke early. `world_reg_europe.c:9-12` documents caps of "~40 total, ~3 per registry" while `:158-159` sets `EU_TOTAL_CAP 500` and `EU_PER_REG_CAP 0` (no cap).
**Fix:** regenerate the numbers from the tools in the same commit that changes the tools; delete or rewrite `SOURCE_REALITY_REPORT.md`; audit the 142 markers against their claims.

**M10 — Three outbound paths bypass the SSRF/politeness floor.**
`native/core/cameraproxy.c:193` · `native/core/camera_stills.c:555` · `native/core/alert_deliver.c:309` (SMTP, out of scope for a URL gate)
`grep -c hostgate` returns **0** for both `cameraproxy.c` and `camera_stills.c`, so neither clears what `hostgate.h` calls "the floor every outbound fetch must clear" — no metadata-range block, no per-resolved-address check — and both also skip `url_override`, evidence capture, content-change detection and host attribution. `cameraproxy.c:186-190` argues uid-indirection makes it safe, but the URL it fetches originates in an `intel_items` row written by `shodan_api`/`insecam_scrape`, and the only validation applied is `is_http_url()` (`:190`) — a scheme check, not a destination check. The correct pattern exists next door: `alertsapi.c:264` validates webhook targets with `hostgate_url_check_strict` at rule-creation time and `alert_deliver.c:168` sends via `http_request`.
**Fix:** route both camera paths through `http_request`; if RTSP/MJPEG needs a raw handle, call `hostgate_url_check_strict()` + `hostgate_addr_check_floor()` explicitly.

**M11 — Tenancy is applied four different ways, and the primary feed applies none.**
`core/intelapi.c` contains **zero occurrences of the string `tenant`** — `/api/intel/items` (SQL at `:294-312`) and `/items/:uid` carry no tenant predicate. `exportapi.c:461` and `nearapi.c:162` use `tenant_id IN (?,'legacy')`; `entityapi.c:67/109/114/168`, `aoiapi.c:639` and `casesapi.c:342` use `tenant_id IS NULL OR tenant_id=?`, which `nearapi.c:157-161` explicitly warns would return **nothing** on `intel_items` (`schema.sql:308`: NOT NULL DEFAULT 'legacy'). Latent only because every `intel_sink_make()` hardcodes `"legacy"` (`content_change.c:1253`, `pipeline.c:227`, `:358`, `scheduler.c:81`, `main.c:149`). `nearapi.h:34` already documents a predicate its code no longer uses.
**Fix:** one `tenant_predicate(table)` helper in `tenantapi.h`; convert all ~250 direct prepares on tenant-scoped tables to use it; add `/api/intel/items` first.

**M12 — `ffmpeg.h` claims a consolidation that is two-thirds done.**
`native/core/ffmpeg.h:20-22` declares `ffmpeg.c` "the single place in the product that spawns a media tool… There is no second one"; `core/media.c:833` still `popen()`s ImageMagick/tesseract through `/bin/sh` **with no timeout**. `ffmpeg.h:183-200` admits it and names the consequence: a wedged tesseract blocks a scheduler worker forever — and since nothing ever sets `ctx->cancel` (L4), nothing can reclaim it.
**Fix:** move `media.c`'s spawns behind `ffmpeg.c`'s timeout-bearing seam, or correct the header.

### LOW

**L1 — `page.total` is hardcoded null on every keyset-paged endpoint.** `native/core/intelapi.c:352`, copy-pasted across `casesapi.c`, `annotationsapi.c`, `aoiapi.c`, `breach_adapter.c`. `next_cursor` answers "is there more" but never "how much" — an analyst on a 50-row default cannot distinguish 51 matches from 5 million. The codebase already knows how: `timelineapi.c` (`meta.totals`), `nearapi.c` (`page.total = n` + a bbox scan-ceiling note), `alert_eval_preview` (`match_count`/`scanned`/`truncated`). **Fix:** run the COUNT(*) over the same WHERE, or replace `total: null` with `total_note: "not computed for keyset paging; use /api/export/intel"` so the absence is a statement.

**L2 — `/api/export/intel` does not return everything.** Its column table omits `body`, `properties` and `keywords` — the verbatim upstream record rule 2 exists to preserve — and no metadata field says a column set was chosen. **Fix:** include them, or add `columns_omitted:[…]` to the envelope.

**L3 — `intel_sink` uid truncates into 512 bytes, silently collapsing distinct records.** `native/core/intel.c:203`. `source_id` is up to 128 bytes (`intel.c:13`), leaving <384 for the key; two records agreeing on their first ~380 bytes hash to the same uid and the `ON CONFLICT` branch overwrites the first. Deterministic and producible on purpose by an upstream (long URLs with a trailing discriminator). **Fix:** check the `snprintf` return; on overflow build `"<source_id>|sha256:<hex>"` over the full key (`searchapi.c:19`, `entitystore.c:60` have hex-digest helpers) and record the fallback in properties.

**L4 — The `volatile int *cancel` contract is inert; nothing in the tree ever writes 1 to it.** `native/core/scheduler.c:84`. Dead at `lib/hpengine.c:659,694` (the paths that set `st->truncated`), `core/media.c:791`, `camera_stills.c:883,1266`, `entity_stats.c:244,293,324`, `breach_monitor.c:299,494,510` and ~25 collectors. `scheduler_stop_background` (`:302`) can only wait out in-flight runs; `JO_SCHED_DEADLINE_SEC`/`watchdog_scan` (`:320-336`) can only print a warning. **Fix:** make it `atomic_int` owned by the queue slot and store 1 from the watchdog and shutdown paths (a `volatile int` is not a synchronization primitive), or delete the field and every dead poll.

**L5 — Scheduler workers park in `pthread_cond_wait` and are never woken on shutdown.** `native/core/scheduler.c:244`. Masked today because `g_inflight` is 0 when the fleet is quiet, so the drain returns promptly — but the drain is not stopping the pool, only observing that nothing is running. **Fix:** `pthread_cond_broadcast` right after `atomic_store(&g_shutdown,1)` and re-check the flag in `q_pop`'s wait predicate.

**L6 — Unchecked `realloc` on the CSV parse path dereferences NULL.** `native/lib/csv.c:22` (and a leak of the previous buffer at `:118`). `field` grows to the largest quoted field in an attacker-controlled body bounded only by `httpclient.c:70`'s 64 MB ceiling; on failure the immediate `field[fl++] = c` writes through NULL and takes the scheduler, the HTTP server and every worker's uncommitted transaction with it. **Fix:** the `char *t = realloc(...); if (!t) { free(field); goto oom; }` shape already used at `lib/hpengine.c:212-214` and `lib/zipread.c:63`.

**L7 — Shared-library discard paths the audit's regexes cannot see.** `lib/csv.c:72-77` iterates only `0..nh-1`, so a ragged row's extra cells vanish with no counter (HP_CSV + 130 VCSV sources). `lib/jsonlist.c:330` drops every record it cannot title, no counter — the file's own comment records a fleet probe measuring **279 sources dropping 232,792 records** for want of a label. `lib/htmlparse.c:130` only recognises `<a ` with a *double-quoted* href, so single-quoted/newline-formatted anchors are skipped across all 291 HP_HTML rows and every `jo_emit_anchors` caller (`html_attr` handles single quotes; the scanner does not).

**L8 — Seven on-demand sources are unreachable from both scheduler and dispatcher.** `native/collectors/sources/geoeo_elevation_points.c:1` + six others — authored as entity pivots, unresolvable because of H7. Dead weight in `/api/status` and the registry.

**L9 — A test harness that ingests an arbitrary local file as intel is registered in production.** `native/collectors/sources/geojson_file.c:8`. An operator-triggered run parses whatever `GEOJSON_FILE` points at and upserts it into `intel_items` as fetched intelligence, with no host and no provenance. **Fix:** `#ifdef JO_TESTS` or move under `native/tests/`.

**L10 — The scheduler's circuit breaker is written only from inside the collector tree.** `native/collectors/sources/collector_repair.c:266`; knobs `REPAIR_BREAKER_THRESHOLD`/`_WINDOW_HOURS`/`REPAIR_QUARANTINE_HOURS` at `:257-259`. A `source_def` the scheduler runs is the sole authority on which `source_def`s the scheduler may run; anyone reading `core/scheduler.c` finds a read with no writer anywhere in `core/`, and quarantining silently depends on that one collector staying registered and healthy.

**L11 — GTFS is a full domain store implemented as a collector, read by two core modules with raw SQL.** `native/collectors/sources/gtfs_jp.c:52` writes 8 tables that `core/isochrone.c` and `core/transitapi.c` read directly. Every other table family has a `core/*_store.c`. `isochrone.c:664`'s `INDEXED BY` makes the query plan part of an unwritten contract — dropping that index turns a working query into a full-feed scan rather than an error.

**L12 — `core/httpd.c` is a second scheduler and a 1,904-line routing function.** `fn()` spans `:599-2502`: 62 exact-path comparisons with the auth gate, method checks, body parsing, thread spawning and reply formatting interleaved. The tenant preamble is copy-pasted **20 times** (`xtid` appears 60 times); `opgate_check(&usr)` 9 times — authorization is a convention repeated by hand, so a route added without its copy is silently unscoped. It holds a global `static db_handle *g_db` (`:59`), reaches into sqlite directly at `:100,220,1116,1149`, and contains its own single-flight table `g_running[16][80]` (`:200-217`, independent of `scheduler.c`'s `g_q.running`), `srcrun_thread` (`:497`) and a full camera fan-out state machine `g_cam` (`:258-360`) with its own 900 s cooldown, registry scan and worker thread. `run_begin()` conflates "already running" with "table full", so the 17th concurrent manual run reports as in-flight.

**L13 — Copy-paste of security-relevant helpers.** `uuid4()` exists **15 times** and **none of the 15 checks `RAND_bytes()`'s return** — on RNG failure OpenSSL leaves the buffer untouched, so the "uuid" is uninitialised stack used as a primary key and, at `tenantapi.c:79`, as a user id. `core/searchapi.c:18` is the single correct copy, and it is the one nobody reuses. The authorization role ladder is byte-identical in six files (`alertsapi.c:30`, `annotationsapi.c:153`, `aoiapi.c:156`, `exportapi.c:239`, `savedsearchapi.c:172`, `uploadapi.c:131`) while `tenantapi.h`, which owns `tenant_ctx.role`, exports no predicate; `can_write()` exists three more times with three different signatures (`aoiapi.c:164`, `breach_monitor.c:525`, `casesapi.c:230`). `core/dbutil.h` exists *specifically* to end this — its own comment notes `ctext()` exists 24 times — and is included by **zero** files in `core/`; 21 private `ctext()` copies remain. Also 13 `iso_now`, 11 b64url, 6 `sha256_hex`.

**L14 — Assorted contract breaks.** `core/exportapi.c:35` includes `../third_party/mongoose.h` and `exportapi.h:185` exposes a callback taking `struct mg_connection *` — the only API module leaking transport upward. `transitapi.h:18-28` documents that the dispatcher has no `status` out-param, so every recognized subpath returns **HTTP 200** with `{"error":"station not found"}`, while `keysapi`/`uploadapi`/`breach_jobs`/`searchapi` in the same tier all take `int *status`. The `intel_items`→GeoJSON Feature contract is written three times (`dataapi.c:168`, `camera_store.c:259` — `dataapi.c:167` says "Identical shaping to camera_store.c", plus a descriptor-driven third at `exportapi.c:718`), each coupled to its own SELECT's column order by fixed index. `core/db.c:2-7` (bottom layer) includes six upper-layer modules to run their migrations at `:351-384`, one of which is a registered collector; `uploadapi_migrate()` is instead called from `main.c:185`; and `breach_monitor`/`camera_stills`/`media` re-call their own migrate at ~7 entry points each — three migration idioms, no single seam. `core/source_registry.gen.c:51` carries a row for `osint-search` with no registered `source_def` — a permanent registry-orphan baselined at 1, i.e. a phantom source in `/api/status` and `/api/layers` forever.

**L15 — Tooling hazards that will re-inject bad data.** `verify_feeds.py:164` dedupes duplicate URLs but `promote_candidates.py:63,83` keys results **by URL** — two candidate rows sharing a URL both receive the single PASS and both get promoted, yielding two ids hammering the same endpoint (not triggered by batch 14; the shipped manifest is URL-unique). `Makefile:122-124` builds the reserved-id list with `|| true`, so a failed grep silently yields an *empty* reserved list; and its input, `lint_sources.py --list-ids`, omits all 601 hp ids — the exact shape of the `UK_CH_FILING_HISTORY` collision. `native/collectors/existing_ids.txt` holds **6,652 ids against 9,436 registered**, and `gen_verified_sources.py:23` still suggests it. `gen_verified_sources.py:214` rewrites the shared `_verified_macros.inc` (`#include`d by 145 files) on **every** invocation, so regenerating one batch re-authors the registration macros behind all of them. `audit_source_exhaustiveness.py`'s docstring writes the strict glob as `hp_*.c` while the Makefile uses `hp*_*.c` — the docstring form would silently drop all 14 `hp2_*` files from the only gated check in the tree. `HP_MAX_SOURCES` is **1024** (`lib/hpengine.c:18`) against 601 rows; overflow drops rows with one stderr line during constructor execution — the identical failure `registry.c`'s 27-line comment says cost them the regional registries once, which is why the main cap is 32768.

---

## 3. Reorganization plan

**The invariant that makes all of this safe:** `REGISTER_SOURCE` / `HP_REGISTER_TABLE` **ids are the stable contract** — they are the DB key (`sources.id`, `intel_items.uid = "<source_id>|<key>"`, `fetch_log`, `quarantine`), the dispatcher key (`osint_lookup` = `registry_get(canon)`), the LLM schema enum, and the API surface. **Filenames, prefixes and directories are not contracts**: `.collector` is never persisted (`core/db.c:106-117` writes id/name/type/category/url only), never appears as a JSON key in any API response, and only **4 of its 85 values** are read by any predicate. Files can therefore be moved and renamed freely; ids must not change, and any rename that changes an id is a data migration, not a refactor.

**The one real hazard is the build, not the ids:** `native/Makefile:64` is a *non-recursive* `wildcard collectors/sources/*.c`. Moving a file into a subdirectory drops it from the link **with no error** — the source simply stops existing at runtime. Step one of any move is converting that glob to a recursive one (or an explicit `SRC_SRC := $(shell find collectors -name '*.c')`) and asserting the registered count before and after.

### (a) Target taxonomy

Partition on the axes the *code* branches on, not on topic. The only three predicates any code reads are `update_interval_sec > 0` (`scheduler.c:124`), `collector == "osint"` (`osint_dispatch.c:74`) and `collector[0] == '_'` (`httpd.c:281`, `scheduler.c:122`).

```
native/collectors/
  lib/                       # registers nothing — today indistinguishable from collectors by filename
      pivot.inc              # ← _jp_osint.inc (200 includers across 65 prefixes, mostly non-Japanese)
      transport.inc trn_common.inc, sanctions.inc, geoeo.inc, aviation.inc, opendata.inc (← od_shared.c)
      social/                # social_fuse.h + the 7 jo_*_run libraries
  pivot/                     # update_interval_sec == 0, UPPER_SNAKE ids   (~1,554 src)
      table/                 # the 29 hp_*/hp2_* HP_REGISTER_TABLE files  (601 rows, the only --strict set)
      handwritten/<domain>/
  feed/                      # update_interval_sec > 0, lower-kebab ids   (~7,270 src)
      generated/<collector>/ # vsrc*/vsrc14/world batches — REGENERATE ONLY, never hand-edit
      handwritten/<domain>/
  pod/                       # .collector starts '_'                      (13 src)
  tools/                     # generators, verifier, planning .md, existing_ids.txt, __pycache__ (out of the build glob)
```

Level 3 = one directory per domain. **Seed the domain names from the 416 files that still carry the pre-flatten path in their header comment** — 26 distinct former directories (`collectors/osint/sources/` ×87, `cyber` ×49, `transport` ×45, `infrastructure` ×36, …), which are exactly the topical `.collector` values. Rewrite those stale headers in the same pass. Naming rule after the move: **the directory carries the taxonomy, the filename carries the upstream** — `feed/handwritten/maritime/emodnet_shipwrecks.c`, no prefix, because the prefix is now the path.

**Prefix → destination mapping**

| Today | Files / src | Destination |
|---|---|---|
| `vsrc_`, `vsrc2_`, `vsrc13_`, `vsrc14_` | 144 / ~6,500 | `feed/generated/<collector>/` (batch tag dies; `vsrc` vs `vsrc2` were *waves inside batch 12* while `13`/`14` are batch numbers — the numeral means two different things today) |
| `csrc14_` | 20 / 1,001 | **gone** — superseded by `vsrc14_` (740 PASS); land the promotion |
| `hp_`, `hp2_` | 29 / 601 | `pivot/table/` (the split is chronological: `hp` 2026-07-30 `5a96f2b`, `hp2` hours later `631a37b`) |
| `trn_`, `od_`, `cyi_`, `eni_`, `mar_`, `geoeo_`, `sanc_`, `tsp_` | 383 / ~394 | `feed/handwritten/{transport,opendata,cyber,energy,maritime,geo,sanctions,spectrum}/` — the `eni`/`geoeo`/`mar` boundary is not a real boundary (M7) and collapses into adjacent filenames |
| `reg_`, `reg2_`, `world_reg_*`, `corp_` | 58 / ~100 | `pivot/handwritten/registries/` next to `pivot/table/` — four implementations of one job become visible neighbours |
| `cam_`, `public_cameras.c`, camera rows in `csrc14_us_cameras_1.c`, `trn_*_cameras.c`, `shodan_cameras_jp.c` | ~20 / ~90 | `feed/handwritten/cameras/` — today spread over 4 naming schemes, 2 `.collector` values and 3 authorship mechanisms; only `core/camera_store.c` downstream knows they are one thing |
| `*_world.c` / `*_world2.c` (63 files) | 63 | dissolved into the domain dirs — this coherent wave is currently scattered into ~55 one-file "families" because it is named by **suffix** while the tree is named by prefix |
| `gnews_*`, `reddit_world_geo`, `world_outlets_*`, `arxiv_feeds`, `gov_agencies_world`, … (18 files) | 18 / 1,120 | `feed/generated/world/` — 12% of the fleet, generated but named topically, so it reads as 13 singleton families |
| ~289 one-file Japan prefixes (`jma_`, `mlit_`, `estat_`, `npa_`, `*_jp.c`, …) | 289 / 509 | `feed/handwritten/jp/<subdomain>/` — 3 of every 4 files buy exactly one source |
| `*_pod.c` + `llm_enricher.c`, `geojson_file.c`, `breach_corpus.c` | 8 / 13 | `pod/` (the `_pod` suffix marks only 5 of 8 today) |
| `_*.inc`, `od_shared.c`, `social_fuse.h` group | 16 | `lib/` — these register **zero** sources, so every "files vs sources" census misreads them as dead |

### (b) REMOVE

| Target | Justification |
|---|---|
| `geojson_file.c` (1 src) | Test fixture registered in production; ingests an arbitrary local path as intelligence (L9) |
| `dam_water_level.c`, drone-nofly (2 src) | Fetch, `free()` unread, emit empty by construction; 144 req/day for guaranteed nothing (M6) |
| `cam_curated_jp.c` (194 rows) | Hand-transcribed catalogue emitted as discoveries with zero network I/O; URLs verified 404 (C2) |
| The 3 GET rows in `social_search.c` `holehe_sites[]` | Fabricate confirmed accounts (C1) — remove until the query actually carries the entity |
| `REVERSE_GEOCODING`, `DOMAIN_HISTORY` | Whole-file copies; the first never calls a reverse endpoint, the second is `domain_age.c` with one string changed (M7) |
| `corp_lifecycle.c:512` `UK_CH_FILING_HISTORY` def | Duplicate of `hp_uk_deep.c:61`, which additionally pages; one is already unreachable (H6) |
| ~40 sources on 53 unresolvable hostnames | No address record under two independent resolvers (M8) |
| 198 never-`ok`-in-5+-runs sources | Triage, not blanket delete — but they are dead until proven otherwise (M8) |
| `core/source_registry.gen.c:51` `osint-search` row | Permanent registry orphan; a phantom source in `/api/status` and `/api/layers` (L14) |
| 20 `csrc14_*.c` | Superseded; 261 of their 1,001 rows are provably dead endpoints (H8) |
| `gen_world_sources.py`, `gen_world_sources_b3.py` | Unrunnable: hardcode `/home/user/JapanOSINT` (`:6`) and `/tmp/claude-0/…` (`:8-11`), no make target. Either fix the paths and wire a target, or delete and declare the 18 output files hand-maintained |
| `native/collectors/SOURCE_REALITY_REPORT.md` | Audits `collectors/osint/sources/`, a directory deleted 2026-06-21, covering 61 of ~9,400 sources — and CLAUDE.md cites it as authoritative (M9) |
| `existing_ids.txt` (6,652 of 9,436 ids), `__pycache__/`, 6 planning `.md` | Build scaffolding and a stale id snapshot living in the shipped-code directory; the stale file is actively recommended by `gen_verified_sources.py:23` (L15) |

### (c) MERGE

**Exact-URL / same-service pairs (delete one id each, keep the more exhaustive implementation):**
`cyi_atlas_anchors.c` + `tsp_ripe_atlas_anchors.c:158` · `geoeo_ioc_sealevel.c` + `mar_ioc_sea_level_stations.c` · `geoeo_ndbc_buoys.c` + `eni_ndbc_buoys.c:27` · Safecast (`geoeo_safecast.c` vs `eni_safecast_devices.c`, different hosts — verify before merging) · NOAA CO-OPS (`mar_` vs `eni_`) · UK flood (`geoeo_uk_flood_stations.c` vs `eni_uk_flood_monitoring.c`) · USGS water (`geoeo_usgs_water_levels.c` vs `eni_usgs_nwis_iv.c`).

**Copy-pasted collectors → one parameterized collector or one hp table:**
- 4 byte-identical `run()` bodies + 13 structurally identical ones → one parameterized `run()` driven by a static row table.
- Korea DART: `reg_kr_dart.c` (`KR_DART`, list.json + company.json) vs `hp_apac_deep.c` (`KR_DART_FILINGS`/`_COMPANY`/`_MAJOR_HOLDERS`) → keep the hp rows, retire the hand-rolled.
- Brazil: `reg_br_cnpj.c` (`BR_CNPJ`) + `reg2_br_cnpj.c` (`BR_MINHARECEITA`, `BR_CNPJ_WS`) + `hp2_southam_brazil.c` (22 rows) → one hp table.
- `world_reg_{africa,asia,china,cis,europe,latam,mena,uk}.c` — 8 files each fanning one query across a static ~26-portal table → one hp table (also removes the false cap documentation at `world_reg_europe.c:9-12`).
- `sanctions_world*.c` → the 40-file sanctions set; `maritime_world*.c` → maritime; `vuln_world.c`/`threatfeeds_world.c`/`ipthreat_world2.c` → cyber alongside `cyi_`/`cert_`; `misc_world.c` (5 src, a literal junk drawer) → dissolved.
- The 10 `*_power.c` (byte-identical until `denki_yoho.h` was extracted; 9 of 10 URLs 404, the tenth frozen since 2025-12-25) → one table + a REMOVE decision on the nine.
- Three GeoJSON Feature shapers (`dataapi.c:168`, `camera_store.c:259`, `exportapi.c:718`) → one `core/geofeature.c`.

**Do not merge (verified false positives):** the 11 `mar_emodnet_*` files (same WFS base, different `typeName`), the `tsp_fcc_{am,fm,tv}` trio (different CGI), `csrc14_*` (zero URL intersection).

### (d) Layer moves in `native/core/`

**Out of `core/` (collector concerns that ended up in the engine):** the `REGISTER_SOURCE` at `camera_stills.c:1726`, `media.c:2141` and `translate.c:924` → thin `pod/` wrappers, mirroring how `core/entity_enrich.c` already keeps its `source_def` out at `collectors/sources/llm_enricher.c:22`. Today the placement rule is "whatever the author chose".

**Into `core/` (control-plane and store concerns that ended up in collectors):**
- `core/source_health.{c,h}` — `source_quarantine(db,id,hours,reason)` / `source_is_quarantined(db,id)`; the breaker write moves out of `collector_repair.c:266` and its threshold/window knobs (`:257-259`) become scheduler tunables. `scheduler.c:135` and `collector_repair.c:173` then share one predicate (L10).
- `core/gtfs_store.{c,h}` — owns the 8 GTFS tables with an ingest API (`gtfs_feed_begin/upsert_stop/upsert_trip/commit`) and the read helpers `isochrone.c` and `transitapi.c` open-code; `gtfs_jp.c` keeps fetch+parse and stops including `core/db.h` (L11).

**Within `core/` (deduplication and chokepoints):**
- `core/authz.{c,h}` — one `role_rank()` + one `can_write()`; deletes 6 + 3 copies from the authorization path (L13).
- Adopt `core/dbutil.h` — 21 `ctext()` copies, and `db_run_stmt()` (which exists because the final `sqlite3_step()` return is routinely discarded, so a failed write reads as success) currently has zero core adopters.
- `core/uuid.{c,h}` — promote `searchapi.c:18`'s RAND_bytes-checked implementation; deletes 15 unchecked copies (L13). Same for `iso_now` (13), b64url (11), `sha256_hex` (6).
- `core/migrate.c` — one "apply all migrations" seam replacing `db.c:2-7`+`:351-384`, `main.c:185`, and the ~21 defensive re-calls; `db.c` stops including six upper-layer headers (L14).
- Split `httpd.c:599-2502` into per-family route tables with **one** tenant/opgate preamble that every route passes through, so an unscoped route becomes impossible rather than merely unlikely; move `run_begin/run_end` (`:200-217`), `srcrun_thread` (`:497`) and the `g_cam` fan-out (`:258-360`) into the scheduler, which already has a queue, a skip-if-running table and family fan-out (L12).
- Route `camera_stills.c:555` and `cameraproxy.c:193` through `http_request` (M10); drop mongoose from `exportapi.c`; add `int *status` to the transit dispatcher (L14).

**In `native/lib/` (the highest-leverage layer work):** port the page-walk + `collector-truncation-notice` machinery down into `jsonlist_emit`/`_verified_macros.inc` (M1 — reaches ~6,500 sources), fix HP_HTML/HP_CSV paging (H5), make the flatten counters thread-local (M5), `strtok_r` sweep (H1), `realloc` checks in `csv.c` (L6), ragged-row and label-less-record counters (L7).

---

## 4. Ordered work plan

| # | Step | Effort | Kind |
|---|---|---|---|
| **0** | **Land or revert the half-landed batch-14 promotion**, rebuild `bin/japanosint`, record the registered count. Right now a build from HEAD and a build from the worktree register different fleets, and the binary is stale. Nothing below is measurable until this is settled. | 1 h | mechanical |
| **1** | **Rule-1 stop-the-bleeding.** C1 (3 rows deleted), C2 (`cam_curated_jp` + `camera_geocode_pod` precision ladder), M6 (2 defs), L9 (`#ifdef JO_TESTS`). These persist false facts into `intel_items` every run. | 0.5 day | judgment (what to delete) |
| **2** | **Thread-safety and the silent-loss bugs.** H1 `strtok_r` sweep + grep guard, H2 first, M5 `__thread` counters, H4 six `n++` + a unit test, L6 realloc, L5 `cond_broadcast`, L13 shared `uuid4`. All local, all verifiable with `make tsan-sched`. | 1–2 days | mechanical |
| **3** | **Make the gates real** — the step that prevents everything above from recurring. Teach `lint_sources.py` to parse `hp_source` tables (fixes the 601-source blind spot, the dup-id miss and the reserved-id chain); call `registry_duplicate_count()` from `--selftest` and fail non-zero (catches H6 automatically); run `make audit-sources` in CI with a **ratcheting baseline at 147**, not `--strict` on 29 files; add a URL-normalizing duplicate check (M7); make `gen_verified_sources.py` read the `verified` column and parameterize the manifest path (H8); fix the `hp_*.c` vs `hp*_*.c` docstring drift; correct CLAUDE.md, `SOURCE_EXHAUSTIVENESS.md:98` and the 142/145 marker count in the same commit (M9). | 2–3 days | judgment (baseline policy), then mechanical |
| **4** | **H3 credential plumbing** — apply the key overlay to the environment at boot and emit `needs-credential` records. Highest ratio of fleet health recovered to lines changed: it un-darkens every gated collector and makes M4 meaningful. | 0.5 day | mechanical |
| **5** | **The exhaustiveness engine work.** M1 (page-walk + notice in `_verified_macros.inc`/`jsonlist_emit`, ~6,500 sources — do this before any per-collector fixes, it subsumes most of them), H5 (HP_HTML/HP_CSV paging + tests), M4 (endpoint-error / needs-credential notice), L7 (counters in `csv.c`/`jsonlist.c`/`htmlparse.c`), then the 147 per-collector findings, M2, M3. Raise `HP_MAX_SOURCES` off 1024 while in the file. | 1–2 weeks | judgment per collector; the macro/engine change is mechanical |
| **6** | **Dead-source and duplicate sweep.** M8 (`last_ok_at`/`records_ever` column + demotion lane, then the ~40 unresolvable / 198 never-ok / 443 zero-record triage), M7 (308 endpoints, 363 redundant registrations, starting with the 7 exact-URL pairs and the 4 byte-identical `run()`s). Expect the fleet to shrink by ~5%. | 1 week | judgment (which of each pair survives) |
| **7** | **The reorg.** Convert `Makefile:64` to a recursive glob **first** and assert the count is unchanged; then `git mv` per §3(a) in one commit per level-1 bucket, asserting `--list-sources` byte-identical after each; rewrite the 416 stale header paths and rename `_jp_osint.inc` → `lib/pivot.inc` (200 includers) in the same pass; move generators/planning docs/`__pycache__` out of the build glob. **No id changes.** | 2–3 days | mechanical, with a hard verification gate |
| **8** | **Core layer extractions** per §3(d): `source_health`, `gtfs_store`, `authz`, `dbutil` adoption, `migrate`, `geofeature`, the three collectors out of `core/`, the camera paths onto `hostgate`, then the `httpd.c` route-table split (biggest, do last). M11 tenancy helper belongs here — it is latent today but becomes live the moment one writer sets a non-`legacy` tenant. | 2–3 weeks | judgment |

Steps 0–4 are ~5 days and remove every confirmed critical/high. Step 3 is the one that matters structurally: without it, steps 5–8 will silently regress, because today nothing in the build, the lint or CI objects to any of the findings in this report.

---

## 5. What was checked and found clean

- **No memory-safety write bug reachable from upstream data.** Bounded `memcpy`, guarded `cJSON_Is*` checks, a body-size ceiling in `httpclient.c:70`, a clamped ZIP inflater. The only allocation defect found is a *read/NULL-deref* on OOM (L6), not an overflow.
- **Rule 1 is genuinely held outside the camera layer.** Every large static table checked is legitimate reference metadata driving a real fetch (`mlit_landprice.c` municipality centroids, `npa_important_wanted.c` prefecture anchors, `phone_intel.c` dial codes, `email_validator.c` disposable-domain classifiers); ~25 files carry an explicit "the curated SEED_* fallback is intentionally not ported"; `real_data:true` appears nowhere in code; the 19 previously-PARTIAL collectors are now honest-empty. **One fabrication finding refuted, one confirmed (C1), one collector family confirmed (C2).**
- **Exact source-id collisions are zero** across all 8,835 `REGISTER_SOURCE` ids (`--list-ids | sort | uniq -d`). The one live duplicate (H6) is cross-mechanism and invisible to that check — which is a tooling gap, not an id-hygiene failure.
- **The hp strict set is genuinely at 0 audit findings**, and hpengine's no-fabrication gates are real and verified by reading the file: shape gate, credential gate (`key_env` unset → 0 emitted, **no request fired**), unresolvable `{qd}`/`{qh}`/`{qu}` token → skip without firing, 404 → honest empty (rc 0), 5xx on page 1 → rc −1. No fixture or fallback path exists in the file. `max_items` defaults to 0 (no cap) and **0 of 601 rows set it**.
- **The write path imposes no caps.** `core/intel.c` stores properties/body/geometry verbatim; `core/db.c` imposes none. `core/pipeline.c` is the model rule-2 consumer: stores every record, bounds only the LLM's *view*, and labels it `records_shown`/`record_count`/`prompt_truncated`.
- **Three read endpoints do rule 2 exactly right**, proving the pattern is affordable: `nearapi` (`page.total` + an explicit bbox scan-ceiling note), `timelineapi` (`meta.totals`), `alert_eval_preview` (`match_count`/`scanned`/`truncated`).
- **The LLM tier is the cleanest layer in `core/`** — `llm_worker.c` includes only `httpclient.h`, `prompts.c` includes nothing, every consumer goes through `llm.h`; one FIFO thread per `base_url` with high/low priority lanes.
- **The alerts subsystem is the best-separated** — one compiled matcher shared by live ingest and preview (the header names "two divergent matchers" as the failure mode it exists to prevent), immutable refcounted rule cache, fail-closed constraint parsing. No structural finding.
- **The breach subsystem's six modules have no overlap** — each boundary is justified in its header; the adapter serves breach rows through `intelapi`'s exact envelope without copying them into `intel_items`.
- **`source_registry.gen.c` / `_dyn.c` is not duplication** — it is a documented per-field overlay where the live `source_def` is the base and `update_interval` always comes from the def. No finding beyond the one orphan row.
- **`osint_dispatch.c` refuses a second registry** — `osint_lookup()` is literally `registry_get(canon)`, and the LLM's service enum is rebuilt from the live registry, so there is zero manual enum upkeep.
- **`hostgate.c` applies the SSRF floor at both layers that matter** — `httpclient.c:199` (URL) and `:126` (every resolved peer address, so a redirect or rebound DNS answer is judged on what was actually connected to) — and is deliberately co-located with `url_host()` so the two answers cannot diverge. The three bypasses (M10) are in modules that never call it, not weaknesses in the gate.
- **`db_worker_open()`'s connection-per-thread rule is correct and its header names the three files that got it wrong.** Each scheduler worker owning its own `db_attach()` is right.
- **The verification tooling is honest about itself.** `verify_feeds.py:74-87` detects HTTP-200 *refusals* (ArcGIS/OGC error documents, `{"error":"access denied"}`) rather than counting them as a healthy 2-key feed — the exact failure mode where a source passes every row-count sweep while fetching nothing. `promote_candidates.py` writes rejects to a TSV **as data**, with verdict and note, and lets the verifier's observed `kind`/`items` overwrite the generator's prediction ("ground truth wins"). That is rule 1 applied to the tooling.
- **Ruled out as duplicates after inspection:** the 11 `mar_emodnet_*` files, the `tsp_fcc_{am,fm,tv}` trio, and all `csrc14_*` (zero URL intersection with the rest of the tree).
- **The headers in this tree are load-bearing and accurate far more often than not.** Only two were found stale: `nearapi.h:34` (documents a predicate its code no longer uses) and `ffmpeg.h:20` (claims a consolidation that is two-thirds done — and admits it 160 lines later at `:183-200`).
- **Do not disturb** during the reorg: `intel.c` as the single sink with `intel_fts_remirror()` published so the two out-of-band `properties` writers cannot leave the index stale; `alert_eval.c`'s shared matcher; `nearapi.c` rendering rows through `intelapi_item_by_uid()` rather than a second row builder; `uploadapi.c` explicitly refusing to be a second blob store; `evidence.c` reusing `audit_events`' hash-chain scheme rather than inventing a second.