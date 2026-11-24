# s_last — API-07 (deadline), OPS-07 (seven tests), SRC-04 (two disclosure shapes)

Branch: claude/peaceful-ritchie-i7e0hj worktree. One commit per item (plus per-test commits for OPS-07).

## 1. API-07 second half — loop-side SQLite deadline

- New `core/loopdl.{c,h}`: `sqlite3_progress_handler` (every 1000 VM instructions) on the event-loop connection, installed once in `httpd_serve()`.
- Armed per request by a thin `fn()` wrapper around the old handler (renamed `fn_dispatch`): arms on `MG_EV_HTTP_MSG` only, disarms after. Deadline = `JO_LOOP_QUERY_MS` through `lib/joenv.h` (default 5000, 0 = off, garbage = default with the one-line complaint).
- State is thread-local and only the wrapper arms it, so the handler is a no-op for worker threads (even one that fell back to the shared handle), startup, migrations and timers.
- On an overrun the query is interrupted (stays interrupted for the rest of the request), whatever the route wrote is cut back out of `c->send`, and the client gets `503` with `Retry-After: 5` and a JSON body `{"error":"query_deadline_exceeded","message":...,"deadline_ms":N}` (CORS headers kept).
- Test `tests/unit/test_loopdl.c`: unarmed heavy recursive CTE completes; armed 50 ms deadline interrupts a 2e9-row recursion in about 50 ms and reports it; connection healthy afterwards; another thread on the same handle never interrupted; env knob default/override/0/garbage; 503 body. Passes.
- `docs/ENVIRONMENT.md` regenerated (478 variables), `env-doc-check` ok.
- `tools/ci_concurrency_gate.sh ./bin/japanosint 1.0`: ok (worst health latency 0.0089 s, limit 1.0 s).
- Limitation stated plainly: the deadline is checked every 1000 VM steps, so a query that sits in one long native step (a huge sort, a single FTS merge) is interrupted at its next check, not instantly; a query that is long because it waits on a lock is not a VM-step overrun and is not covered (busy_timeout owns that).

## 2. OPS-07 — tests for the seven modules

All in `native/tests/unit/`, each includes the module under test (same harness as neighbours), real behaviour and edge cases:

| test | what it pins |
|---|---|
| `test_content_change.c` | normalizer (timestamps, relative times, epochs, tokens, counters are noise; phone numbers, advisory ids, bare counts, real link changes survive), diff (equal, add/remove, null sides, coarse >800-line path flagged truncated), capture states (baseline/unchanged/counter tick/changed, api type skip, `watch_content` 0/1, empty, oversize), http hook gating, retention sweep (age + per-key KEEP) |
| `test_vocabapi.c` | closed lists carry a source and agree with the schema CHECKs (listed values accepted, an unlisted one refused), open lists counted/ordered/tenant-scoped (legacy shared, other tenant never leaks, NULL/'' skipped), TTL memo, refresh, per-tenant slots and eviction, 600-value bound stated in-band |
| `test_sweepapi.c` | exact id set, NULL for non-sweeps, cold read bounded by `JO_SWEEP_LIMIT`, stored geometry wins, prebuilt cache served and `_build` bypasses it, unified rebuild (stations + stitched lines + counts, no leakage between sources) |
| `test_breach_meta.c` | slug folding (NFKD table, separators, unstorable names, malformed UTF-8, buffer bound), count parser, seed TSV (skips counted, bounded sample, NULL for unreadable counts, idempotent), manifest upsert/NULL-for-silence/bad files, source list join and display fallback |
| `test_unified.c` | normName, allSettled (missing/failed upstream contributes nothing, partial emissions of a failed one not merged), kind tagging, fill-only merge, sources list, coordinate-key fallback by precision and name, raw merge, filter-before-dedupe, post, `unified_capture` |
| `test_mlit_ksj.c` | against a local python stub: alias fallback, null for absent fields, midpoint/centroid/preserve geometry, malformed vertices skipped, stable indexes past dropped features, N05 status + `abolished_only`, N07 default route type, bare-array root, env/mirror order, failure semantics |
| `test_exportapi.c` | plan (400/403/500/501, literal filename, breach source+role gate), tenancy per kind, CSV BOM/quoting/formula guard (numbers untouched), GeoJSON skipped-no-geometry counted, caps stated in all formats and the audit row (exactly cap rows is not truncated; skips cannot hide truncation), filters/cursor/injection literal/refused sorts, breach redacted projection (no hash), failing writer stops the walk and is audited as aborted |

Bugs found by the tests and fixed:
1. **content_change**: a compact JSON document (`{"updated":"2026-01-02T03:04:05Z"}`) is one whitespace token, so its timestamp never reached the date rule and every fetch of a watched JSON payload reported a change. For JSON-looking input the quote is now a token boundary. Hashes of watched JSON documents therefore change once (the documented CC_NORM_VERSION caveat: one false change per such key after deploy; default-watched sources are scraped HTML and are unaffected).
2. **sweepapi cold read**: `capped` was `n >= LIMIT`, true when a layer held exactly LIMIT rows. It now reads one row past the limit and is true only when a row was actually left out.
3. **mlit_ksj**: when no env/mirror URL could be read, the collector fell through to "0 rows, rc 0" (an empty source, rule 1). It now returns -1 (scheduler files the fetch-failure notice); an upstream that answered with an empty/filtered collection is still an honest 0. Stale comment in `mlit_n02_stations.c` updated.
- exportapi, vocabapi, unified, breach_meta: no defects found.
- Not testable in-process: `JO_NO_CONTENT_CHANGE` and the size knobs in content_change are read once per process (cached statics); the oversize case is tested with a 4 MiB+1 body against the default instead.

## 3. SRC-04 — two undisclosed shapes

Root cause (both): the fetch-failure notice in `hp_run` fired only on `first_status != 200 || st.unreadable`.
- **Cookie wall** (`JO47_FR_D507DF73...`, live: 229 B text/html, `bot_mitigation_cookie`, `<noscript>This website requires JS enabled and cookies</noscript>`): only the JSON driver ever set `unreadable`; the XML/CSV/NDJSON drivers read the page in their own mode, found no records, and returned rc 0 with nothing filed.
- **ArcGIS error envelope** (`JO53_AGS_SERVICES_FREIGHT_ROUTES_CFEC`, live: `{"error":{"code":499,"message":"Item does not exist or is inaccessible."}}` with HTTP 200): the engine detected it (`upstream_error`) and treated a 4xx code as an honest empty "like an HTTP 404", but a real 404 files a notice and this did not.

Engine fixes (`lib/hpengine.c`, every row of the shape):
- `hp_body_is_html_doc()` (root only, after BOM/whitespace) + `hp_classify_refusal_body()` (one place for rate-limit / bot-wall words; added the cookie wall's own phrase "JS enabled and cookies"). For XML/CSV/NDJSON rows, a 200 whose root is an HTML document is not parsed in the row's mode: `unreadable` is set and the notice is filed, named a bot wall only when the page's own words say so.
- Error documents: the notice now also fires on `upstream_error`, quoting `upstream_error_code` and `upstream_error_message`, with an outcome/remedy/title that say what the upstream said. rc is unchanged (5xx or no code: -1; 4xx: 0). An error document that says "no matching"/"no data available" with code 404 is still an honest empty (same body-word rule as the transport 404).

Tests (`tests/hpengine_test.c`, `make hptest`): 45c cookie wall on an XML row (one notice, named bot wall, `body_parsed:false`), 45d maintenance page behind BOM/whitespace (failed fetch, not called a wall) and an HTML page in a CSV row, 45e controls (feed embedding html, feed with zero items: no notice), 45f ArcGIS 499 envelope (one notice quoting code and message, rc 0), 5xx envelope (rc -1 + notice), 45g "no matching" envelope (honest empty) and `error:null` beside records (read normally). Four older assertions that said an error document "stores nothing" now say "no finding, only the one notice" (the intent was never "no notice").

Live (`./bin/japanosint --run <id>`, scratch DB, first try, network up):
- `JO47_FR_D507DF73_2260_4D04_B6B6_4F2A23EC7E25`: `rc=0 records=1 stored=1 bytes=229 requests=1`; stored `collector-fetch-failure` "stored nothing: a bot wall answered 200 instead of the data".
- `JO53_AGS_SERVICES_FREIGHT_ROUTES_CFEC`: `rc=0 records=1 stored=1 bytes=154`; stored `collector-fetch-failure` "the upstream answered 200 with an error document (code 499)", `upstream_error_message` "Item does not exist or is inaccessible.".
- These two rows still hold no data (the cookie wall needs a browser; the ArcGIS item was removed). Not re-pointed: that is unmeasured, as fix_src.md already said.

## Verification (from native/, scratch JO_DB under the scratchpad)

- `make -j8`: 0 warnings.
- `make hptest pagewalktest rsstest authtest lint-sources source-floor registry-floor audit-sources env-doc-check`: exit 0 (registry 105,228 >= 105,228, 0 duplicates/never-runs/kind-mismatches; audit-sources 0 findings; env doc current).
- `tools/ci_concurrency_gate.sh ./bin/japanosint 1.0`: ok (item 1).
- `make unit`: see the final line of this file.
- `make unit`: first full run failed `test_routes` (my rename of the dispatch function hid it from the matcher parser; fixed: dispatch stays `fn`, wrapper is `fn_deadline`). Second, uninterrupted run after the fix: all 126 tests, `UNIT_EXIT=0`, 0 FAILED (log: scratchpad/last_unit2.log). Gate and `test_routes`/`test_export_stream` re-run on the final binary: ok (health latency 0.003 s).
- (The single "FAILED" string in that log is a sched log line from a test of refused sink records, EMIT-FAILED, expected output.)
