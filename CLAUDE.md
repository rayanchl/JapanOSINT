# JapanOSINT — house rules

Rules and pointers only. Dated measurement history lives in `docs/history/`
(moved verbatim on 2026-11-16; each section below names its file). Where history
contradicts this file, this file is current. Both core rules are one idea: what
we display is exactly what we actually obtained.

**Registered source count: 105,228** (measured 2026-11-23, `make source-count`; the `geojson-file` test fixture is now behind `JO_TEST_PODS`).
That number is dated; `make source-count` and `native/tools/registry-floor.txt`
(plus `source-floor.txt`) are authoritative. A count is a reading of one tree:
measure after every merge and re-record both floors (`make source-floor-record
registry-floor-record`), never add arithmetic. Ledger: `docs/SOURCE_COUNT_LEDGER.md`,
history `docs/history/source-count-ledger-notes.md`.

## 1. Never fabricate data

Real fetch or honest empty. No fixture rows, placeholders, hardcoded source names
emitted as findings, or seeded values standing in for a failed fetch.

- A failure degrades to an explicit `error` / `not_found` / "needs credential" note, never invented content.
- A refused fetch must not look like an empty source (`records=0 stored=0 rc=0`). `lib/hpengine.c` stores one upsert-keyed `collector-fetch-failure` per (source, entity); `core/scheduler.c` (`sched_disclose_fetch_failure`) does it for every other collector (VJSON/VGEO/bespoke).
- The scheduler's notice also fires on non-zero rc with no record even if every host answered; it states only what the run exposed and never invents an `http_status`.
- A refusal can wear a success status: rate-limit pages -> measure the budget, then a gap in `core/hostgate.c`; bot walls (Imperva) -> `core/host_agents.def`. Match the refusal's own words only. A gated/maintenance/window row is reported as such (`GATED`, `POD`, `WINDOW_EMPTY`), not as a defect. A 2xx body that is not a feed is a failed fetch; collectors must not return 0 for an upstream that always has data.
- An upstream's own "nothing matched" (204, or a 404 whose BODY says so, FDSN `nodata=404`) is an honest empty; discriminate on body words, never status.
- An anchor calendar whose labels are all under 3 chars files `anchors-unlabelled`; never lower the label minimum.
- A nested record (every field an object) stores nothing in VJSON; use hpengine dotted `id_keys`/`title_keys` (`EGOV_LAW_LIST_ALL`). `,` chooses so one row can key on both wrapped and flat shapes.

History: `docs/history/rule1-fetch-failure-disclosure.md`.

## 2. Never discard data — a source that is called must be used exhaustively

Use everything a call returns: every record, field, page, and the detail endpoint behind each hit. Nothing dropped at a seam between collector, sink, API and client. Detail: `docs/SOURCE_EXHAUSTIVENESS.md`.

- A consumer that cannot take everything may bound its own view but must state in-band how much it shows of how much exists. Silent slicing is the violation.
- Anything left unused is reported as data (`collector-truncation-notice`), not a log line.
- Page through `jsonlist_emit_paged()` (VJSON), `pw_walk`/`pw_walk_max` (`lib/pagewalk.c`) or hpengine paging. `jsonlist_next_page()` is the one per-page decision: server next link, else cursor + declared page size, else page number when the upstream's total says more.
- `make audit-sources` must stay at 0 findings (strict set `collectors/pivot/table/hp*_*.c` + `collectors/feed/generated/hp1[0-9]_*.c`, and the rest of the tree). A new finding is a regression. When you add a walker, add it to the scanner in the same change.
- Deliberate exceptions carry `/* exhaustive-ok: <reason> */` on the flagged line itself (a marker in the comment above is ignored).
- A limit spelled into a URL (`limit=200`, `$top=2`) with no paging is a discard the C scanner cannot see (`tools/audit_url_limits.py`, read not gated). A generated row inherits its probe's limit; set page size x `page_max` against the detail budget.
- Ask the upstream's default ORDER: an oldest-first slice under a limit is a lie about what the source holds (STAC `sortby=-datetime`). A keyset cursor beats a sort for drifting heads.
- `hp_detail_admit()` spends 1,000 hops or 120 s per run; unreached records carry `_detail_pending` and a `detail-truncation` notice. A detail_url equal to the row's own url is worthless (`detail-self-hop` lint, marker `detail-self-hop-ok`).
- A declared page size is not evidence the server honours it. Offset/page walks over live data need a stable sort on the row's own id and the offset bound in the first URL; a small collision there is a skip until measured otherwise.
- A ceiling can be the upstream's, served as an empty 200 (Japan Search `from+size<=2000`) or 408/400; probe one record past the ceiling before raising `page_max`.

History: `docs/history/rule2-exhaustiveness-and-paging.md`.

## 3. A registered source must actually be reachable

`hp_run` reaches a row two ways: an entity token (`{q}`, `{qd}`, `{qh}`, ...) in URL/body (entity pivot), or `interval > 0` (scheduled). A static URL with interval 0 never runs.

- Converting a scheduled row to a pivot needs `interval = 0` plus a `{q` token; no collector relabel. `registry_is_entity_pivot()` in `native/registry.c` is the one rule (dispatcher and `service_vec` share it).
- `make registry-floor` fails on `NEVER-RUNS` and `KIND-MISMATCH` rows. What a source IS is the explicit `.kind` (`SOURCE_KIND_FEED|PIVOT|POD|PROBE`, read only via `source_def_kind_of()`); never test the collector's `_` prefix for behaviour (pods and `PORT_SCANNER` declare POD/PROBE; a `_` collector must match).
- Check: `python3 native/tools/audit_batch_reachable.py docs/candidate-sources-batch*.txt`.
- `osint_lookup()` resolves a unique case-fold match; an ambiguous fold is refused. Dispatch resolves a misspelling to the nearest entity pivot only.
- Every source self-registers (`REGISTER_SOURCE` or `HP_REGISTER_TABLE`); there is no separate "service" type. A row with no `.url` is rejected at registration (named on stderr); `index_url` rows still need one.

History: `docs/history/rule3-reachability.md`.

## 4. Fetching is not emitting — prove the second one

Probe PASS says an endpoint answers; it does not say the engine turns records into `intel_items`.

- Run the batch gates, then `python3 native/tools/audit_batch_emit.py <manifest> --bin ./bin/japanosint --jobs 6` before believing a batch; `DROPS_EVERYTHING` is the verdict that matters. `make batch-gates MANIFEST='../docs/candidate-sources-batch<N>.*.txt'`.
- Pivot rows: manifest-driven `audit_batch_emit.py`, never the registry sweep (asks no entity).
- `audit_registry_emit.py --match` is a regex on SOURCE ID; zero rows selected reads as clean. `SLOW` is unmeasured; re-run each `FETCH_FAILED` once.
- Gate and engine disagree: reproduce the tool's exact invocation; check the build's exit before trusting a measurement.
- `audit_registry_emit.py` pins a copy of the binary at startup so a concurrent `make` cannot break it (`--no-pin` opts out).

### 4b. Emitting is not storing either

`records=N` counts `emit()` calls; the sink upserts on `remote_key`, so colliding keys report a healthy N and store one row.

- Read `stored=` on the run line next to `records=`; `stored` is DISTINCT-UID. `UID-COLLISION` means the row's identity is wrong or the walk drifts. `notices=N`, `failed=N` follow.
- `id_keys` is a manifest declaration, not the upstream's identity. A dimension (country on a weekly series) discards the series. Declare identity unique per record, not per group.
- In `id_keys`, `+` COMPOSES and `,` CHOOSES (`a+b,c` = a+b else c). Use `+` for tuples. A single `--run` cannot see a key unique in one page and repeating later; measure over many pages and the whole cycle.
- A name is not a measurement: ask the upstream for one value of the key (`uin` restarts each session; `noe_id` covers several investigations). Prefer the upstream's own row id (`:id` needs `$select=*,:id`; `_about`).
- The in-page content-hash guard keeps differing records and collapses byte-identical ones. Where the hash fallback beats a "real" id (CPD case numbers), keep the fallback; compare against the row's current loss.
- VJSON precedence list (`id`, `uid`, `guid`, ...) finding nothing keys on the first scalar; use `VJSON_KEYED` / `VJSON_IDKEYS` (`_vjson_idkeys.inc`, same `+` semantics). Delete the `VJSON(...)` block you replace.
- A collision that is really a skip is the worst shape: an unordered offset walk serves one record twice and skips another with no trace.
- UID-COLLISION of exactly one page size on a `page_walk` row is the walk's page-1 probe; bind `&page=1`, do not widen `id_keys`. An inclusive cursor repeats one record per boundary; same advice.
- Parsing: check one record's `properties` the first time a non-comma CSV ships; the record count says nothing about fields.

### 4c. Two tools reading one manifest must read it the same way

- `native/tools/manifest.py` is THE manifest parser; it reports duplicate keys, never resolves them. Do not write another. A stray `\;` swallowing the next opt is rejected by name.
- `native/tools/jo_tls.py` is the one TLS context for tools (verification stays on; only the cipher level is lowered); a tool that cannot see what the engine handles reports live sources dead.
- Read the TLS error, not the word "TLS": `no_local_issuer`, `expired`, `hostname_mismatch`, `self_signed`, `unrecognized_name`, `handshake` have different remedies (`http_last_tls_cause()`). Missing intermediates go in `native/data/tls/extra-intermediates.pem` (never self-signed).
- In a proxied container TLS terminates at the egress gateway, so a `no_local_issuer` reading taken here is about the gateway. Control with a host known to work (and the publisher read directly with `openssl s_client`) before trusting a TLS cause. Peer resets can be OUR egress (`ws_closed_mid_exchange`, `/__agentproxy/status`).

### 4d. Answering is not answering THE QUESTION

- An entity pivot whose API ignores its filter returns an unrelated collection with HTTP 200. Run `python3 native/tools/probe_hp_batch.py docs/candidate-sources-batch<N>.*.txt --check-filter` once per batch; `FILTER_IGNORED` is fatal (drop or re-point).
- A Socrata `/api/catalog/v1` needs `domains=` and `search_context=`; an FDSN node runs only some services (ask `/fdsnws/<service>/1/version`).
- Send a control parameter (`frobnicate=7`, `sort=notacolumn`) before believing a sort or filter works. A 400 tells you a shape was rejected, only an unknown field inside a well-typed value tells you the order is honoured. An unrecognised parameter can FILTER (Gazette `sort-by`); compare the upstream total before and after.
- Read the body on a 200: error pages, PHP warnings, `code=500` envelopes and maintenance pages arrive as successes.

History: `docs/history/rule4-emit-store-identity.md`, `docs/history/manifest-parser-and-tls.md`.

## Engine subtleties for row authors

- `page_size` makes `page_param` an OFFSET. Declare it only for offset parameters; on a page-NUMBER API (`page`, `pageNo`, `page[number]`) put the size in the URL. `make lint-sources` fails `page-size-on-page-number`; marker `page-offset-ok: <measurement>` for a true offset.
- `page_zero_based=1` for 0-based page APIs; check where an API starts. A URL that already binds `page_param` is replaced, not appended.
- A row declaring no paging makes ONE request; `page_walk=1` (HP_JSON) walks it via `jsonlist_next_page()`; it moves only cursors whose size sibling is in `PAGERS` (`lib/jsonlist.c`: `rows/start`, `limit/offset`, `per_page/page`, `count/start`, `resultRecordCount/resultOffset`, `maxFeatures/startIndex`). Read the row's own opts and construct before concluding from its URL.
- `array_path=.` for a bare root array (otherwise the densest object array is mined); `a+b+c` emits sibling arrays; an array of bare strings is a record set (`title_keys=value`, `id_keys=value`).
- `id_keys`: `+` composes, `,` chooses; dotted paths allowed in hpengine; XML repeated children are indexed (`name`, `name.1`); name a bare array element (`establishment_type.0`).
- `csv_delim`: `tab`, `pipe`, `semi`, `comma`, `ws`, `lit:<token>`, or one literal char; an unknown WORD reads as comma with a stderr line. `make lint-sources` fails `csv-delim-name`. `csv_comment=#`; `csv_no_header=1` for human-laid-out sheets (header not row 1, merged cells, sheets of one workbook differ; positions are fragile).
- `occurrence_rows=1`: a register of EVENTS with categorical columns publishes distinct events as identical bytes; keys a repeat on its occurrence number. Catalogues keep the content-hash dedupe. The author declares which.
- `mode=ndjson` for line-delimited JSON; `mode=html` + `table_must` (`table_index`, `table_skip_rows`) for HTML tables; `detail_html_kv=1` for HTML detail pages; JSONP unwrapped automatically.
- `xlsx_sheet_all=1` and `zip_all_entries=1` (JSON/NDJSON only) read every sheet/member; `id_keys` must be unique across the container.
- `row_header_path` names the header array of a JSON table so row keys are not positions.
- `index_url` + `index_href_must` for a file renamed each edition; the row still needs a `.url`.
- Never freeze a date into a URL: `{date}`, `{date-7d}`, `{date:%d-%m-%Y:-2}`, `{ago:N}` (RFC 3339 instant, for append-only upstreams). `date_back=N`/`date_step` step back on 4xx/empty; `date_field` stamps records whose rows carry no date.
- `next_path`: `key=value` segment (`links.rel=next.href`), `$last` segment, numeric cursor only with a `next_tmpl`. A KEY MAY CONTAIN A DOT (`@odata.nextLink`); resolvers fall back to the literal key.
- Page ceiling: one number, one knob: `jo_page_max()` = 20 under `$JO_PAGE_MAX` (`lib/pagewalk.c`); `jo_page_max_for(declared)` lets the operator brake a declared `page_max` both ways. Leave headroom above a measured need: `page_max` exactly equal to the pages needed files a FALSE truncation notice.
- Ceiling x page size is never the upstream's total. Declared totals (`records_available`, basis "upstream declared this total") are the authoritative signal; `more_pages_pending` is not. Totals may be strings (OData v2 `d.__count`) or nested (`result.count`, `hit`).
- `page_resume=1` cycles a collection one ceiling per run via `source_ctx` `resume_in`/`resume_out` (stored in `source_sched_state.resume_token`; `lib/` has no DB code). A walk that completes stores no position (a no-op). Refused for entity runs, `{date…}`/`{ago…}` rows, `{page}`/`{offset}` tokens in path/body, and a stored position on another host. Use an ascending sort (new items land at the tail); never on a recency row, nor under 6 h cadence. Check that page 2 is not refused (ENA `offset`, EBI 408) before cycling. A failed page does not advance the position.
- `window_days=N` with `{wstart}`/`{wend}` for a date-only slicing axis: walks FORWARD from `window_from`, pins to the trailing window, a window not served does not advance. Valid only where every record is dated; compare counts with and without the predicate. A window exceeding one read discloses itself; size windows from DAILY counts.
- Declare `title_keys`/`id_keys` for anything not named `name`/`title`/`id`; a generated row's keys are guesses until measured (`tools/audit_idkeys.py`).
- ArcGIS: `orderByFields=<objectIdField> ASC` (read layer metadata). WFS needs `sortBy` for `startIndex`.
- `filter_query=1` filters locally: wrong when the server already filters on `{q}`. A record's `body` is text, never JSON. Detail hops carry the row's headers. Use `lib/seenset.c` and `jo_strcasestr`/`jo_memmem` (`lib/jocore.h`), never glibc's. Later-page failures are disclosed (`failed_page*`) except after a short page.
- Adding an `hp_source` field: add it to `STR_OPTS`/`INT_OPTS` in `native/tools/gen_hp_batch.py` in the same change (third place that gets forgotten).

History: `docs/history/engine-row-authoring-history.md`, `docs/history/batches-and-tooling.md`.

## Process rules (standing instructions)

- Batch numbers are claimed by whoever merges first: check `git ls-tree -r --name-only origin/main | grep -oE '(batch|hp3b)[0-9]+' | sort -u` before generating, re-check at merge time, renumber BEFORE merging main. Read the beat, not the batch number.
- After a beat ships, the C is the maintained copy; manifests are probe records. Regenerating a shipped beat reverts verified repairs; generators refuse to overwrite (`--force` only if the manifest is newer). A staging file for a shipped batch is history, not a to-do list (batch 45 -> 49).
- One probe is one reading: re-run failures; a 403/404 can be a User-Agent or a bad afternoon. Bisect the agent (engine, without "collector", bare `JapanOSINT/1.0`, `Mozilla/5.0 (compatible; …)`) before writing off a 403/444/502 row.
- Measure before changing a key, a URL, a sort or a window; a URL changed after validation is unvalidated (`&amp;`, siblings move with it). A literal `&amp;` is not always a bug; measure both. Concatenate multi-part C literals before reading their last char.
- Pass `--bin ./bin/japanosint` to `batch_exclusions.py`; the regex id set is an approximation. Run `make lint-sources` before believing a re-point is a NEW source (`dup-endpoint`, `detail-self-hop`, twin rows).
- A new beat's group goes in `native/tools/gen_collectors_def.py` (OVERRIDES/KEYWORDS/CATEGORY_GROUP), never hand-edited in `core/collectors.def`; `make collectors-def` then rebuild.
- Every UA/contact decision lives in `core/host_agents.def` (one line per host: `HA_NOCOLLECTOR`, `HA_CONTACT`, `HA_RCORP`, `HA_COMPAT`, `HA_BROWSER`, `HA_SEC`); `./bin/japanosint --host-agents` prints it; the probe reads the same file. Admit a browser profile only after bisecting; no URL in a header (lint reads it as an endpoint).
- **SEC contact is a credential, `SEC_CONTACT`** (or `$SEC_CONTACT_UA` full string, which wins): `HA_SEC` is a template filled by `http_contact_ua()`; set via `PUT /api/keys/SEC_CONTACT` (an `@` address, no CR/LF). Unset, no request goes to an SEC host: `http_request()` refuses with `http_credential_refusal()` and a `collector-status-notice` (`GATED`). No personal address belongs in the tree.
- macOS CI job cancelled at 15 min by the runner while the other five jobs are green and the change was macOS-green on a previous head: merge and say so in one comment; never an empty commit to re-trigger.
- "CI is red" is a question (`total_count: 0` jobs means the workflow file is invalid); "just flaky" is a question too (an intermittent assertion means the invariant is intermittently false; instrument the quantity). Do not widen slack.
- `make unit` links one `ld -r` object, never an `ar` archive (constructors dropped, empty registry); build one-off tests via `tests/unit/run.sh`, not bare `find obj`. CRLF checkout breaks scripts: `git ls-files -z '*.sh' '*.py' | xargs -0 sed -i 's/\r$//'`. Set `JO_DB` to a scratch file for gates. `db_open` stamps a seed fingerprint; `JO_DB_RESEED=1` forces the full pass.

History: `docs/history/ci-and-tests.md`, `docs/history/tls-and-agents.md`, `docs/history/batches-and-tooling.md`.

## Commands (run from `native/`; use `JO_DB` on a scratch file)

```sh
make                 # full build (-Wall -Wextra), tree at 0 warnings
make selftest        # DB integrity, schema objects, llm probe
make unit            # tests/unit/run.sh against a scratch DB
make hptest          # offline check of the hpengine guarantees
make authtest        # JWT / tenant auth
make pagewalktest    # paging + disclosure engine
make htmlparsetest   # the one anchor scanner
make rsstest         # dead feed = failed fetch, empty feed is not
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards, page-size/csv-delim/detail-self-hop
make source-floor    # collectors still register (tools/source-floor.txt)
make registry-floor  # the binary's registry: no DUPLICATE/OVERFLOW/NEVER-RUNS
make source-count    # the authoritative registered count
make audit-sources   # discard patterns, 0 findings expected
make collectors-def-check   # every collector key has a group
tools/ci_concurrency_gate.sh ./bin/japanosint 1.0   # event loop stays free under load
```

CI also runs TSan and ASan builds and the client's `vitest`/`vite build` (`.github/workflows/ci.yml`).

## Where things live

```
native/source.h                    the ONE data-acquisition ABI (source_def + intel_sink)
native/lib/hpengine.{c,h}          declarative deep-record collector engine
native/collectors/sources/*.c      hand-written collectors, one file per family
native/collectors/feed/generated/  generated scheduled feed collectors (vsrc*)
native/collectors/pivot/table/*.c  hpengine tables (hp*, hp2*, hp3*) — the strict set
native/collectors/pod/*.c          enrichment/maintenance pods
native/core/                       db, http, intel sink, dispatcher, pipeline, HTTP API
native/data/tls/                   the intermediates our upstreams omit (core/catrust.c)
native/tools/                      lint_sources.py, gen_hp_batch.py, probe_hp_batch.py, audits
docs/                              plans, evidence TSVs, docs/history/ (dated history)
```

The Makefile globs `collectors/` recursively; do not flatten it. Batch tooling table (probe, exclusions, pagination, reachability, emit audits, feed discovery, WordPress sweep, `cleanup_stale_uids.py`) is in `docs/history/batches-and-tooling.md`.

## Cadence policy

Every source declares `update_interval_sec`; one operator policy (`core/cadence.{c,h}`, `app_settings` keys `sched.global_interval_sec`/`min`/`max`, `GET`/`PUT /api/sources/cadence`) sits above. Compiled default: global 6 h (`CAD_DEFAULT_GLOBAL_SEC`); bootstrap vars `JO_SCHED_INTERVAL_SEC` etc. apply to an EMPTY database only. Scale with `JO_SCHED_WORKERS` (6 h policy needs about 24+).

- `sched_row_interval()` is the one composition: per-source override (`core/source_interval.{c,h}`, `/api/sources/:id/interval`, settings key `interval_sec`) > `cadence_apply(sched_state_base(...))` (measured freshness inside, policy outside). Entity pivots (interval 0) are never touched.
- Rows with `adaptive_interval=1` are re-bucketed from the newest `published_at` after a run that worked; a refused fetch changes nothing.
- `sched_next_due()` is the one next-due function; `./bin/japanosint --schedule` prints it. Maintenance pods (kind POD) are exempt from the policy. A row the queue has no room for stays due. Real-time feeds (`wolfx-eew`, `jma-tsunami`) are held at the policy until given their own interval.

History: `docs/history/cadence-and-scheduling.md`.

## Source pages API and per-source settings

- `core/sourceapi.c`: `GET /api/sources/search`, `/:id/detail`, `/:id/summary` (facts only; on LLM failure status plus facts, never prose), `/api/collectors[/:key]`. `sources.status` is written after every run. Every collector row has a key (hpengine default: table file basename); there is no `osint` collector; `core/collectors.def` is generated.
- `core/source_settings.c`: the key set is CLOSED (typed, bounded, bound re-checked on read); `enabled` is separate from the value; reads plain-auth, writes operator-gated and audited (`source.setting.put`). `http_body_ceiling_bytes` raises the 64 MB body ceiling per source (still clamped by `$JO_HTTP_MAX_BODY`).
- Plan and notes: `docs/source-pages-revamp/`. History: `docs/history/source-pages-api-and-settings.md`.

## Embeddings, thresholds, tenancy, CORS

- One vector library `lib/vecindex.{c,h}`; never `ALTER … RENAME` a vec0 table; never mix models in one index. `embedding-backfill` (`core/embed_pod.c`) embeds on a worker thread; queries never embed on the event loop.
- Every similarity threshold lives in `core/thresholds.{c,h}` and is read with `threshold_get("<key>")`: never a module `getenv()` or bare constant. Resolution: DB override > env var > compiled default; edit through `/api/admin/thresholds` (operator-gated, audited). A new cut-off gets a row there. Cut-offs on the production model must exceed about 0.85 to reject anything; do not change values without labelled positives (`tools/measure_thresholds.py`, `docs/measured-thresholds-2026-10-23.md`).
- Tenancy: OSINT records are one shared corpus (tenant `legacy`); what a workspace authors is readable by its members, edits stay with the author, nothing crosses workspaces; case VIEWERs read only. Search history records committed searches only, never per keystroke.
- Infra audit guarantees (SSRF/hostgate, `core/netjail.c`, bounded LLM lanes, hash-chained audit log): `docs/history/infra-audit.md`.
- CORS is an explicit allow-list: `JO_CORS_ORIGINS`; unset sends no CORS header.

History: `docs/history/embeddings-and-thresholds.md`, `docs/history/infra-audit.md`.

## Pointers

`docs/SOURCE_EXHAUSTIVENESS.md` (rule 2), `docs/SOURCE_COUNT_LEDGER.md`, `native/collectors/SOURCE_REALITY_REPORT.md`, `docs/OPEN_ITEMS_2026-10-20.md`, `docs/registry-health-sweep-2026-10-25.tsv`, `docs/source-scheduling-audit-2026-11-14.md`, `docs/DOCS_PRUNE_2026-11-16.md`, and `docs/history/*.md` (14 topic files named in the sections above).
