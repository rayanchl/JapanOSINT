# Engine bug hunt (native/main.c, registry.c, core/*, lib/vecindex.c, collectors/pod/*)

Read-only review, 2026-11-14. Every finding below was verified by reading the code path (file:line given). Confidence is stated per item.
Areas read in depth: scheduler.c, cadence.c, source_interval.c, intel.c, httpclient.c, hostgate.c, llm.c, llm_worker.c, osint_dispatch.c, pipeline.c (phase 1/2 + run_tasks), alert_deliver.c, alertsapi.c (channel validation), collector_repair.c, anomaly_triage.c (prompt part), retention_pod.c, ffmpeg.c (spawn/reap), catrust.c, thresholds.c, registry.c, progress.c, vecindex.c (identity/writer), content_change.c (hook), camera_stills.c (grab).
Areas only skimmed / not substantiated: simhash.c internals, fts_schema.c, entitystore.c, service_vec.c, image_vec.c, breach_monitor.c, station_clusterer.c, embed_pod.c (delta walk), translate.c (outside the item below), main.c.

---------------------------------------------------------------------------
## Findings

### ENG-01  HIGH  A full queue drops a source's cycle AND advances its due time by a whole interval; the boot ramp makes this the normal case at fleet size
- core/scheduler.c:1029 (SCHED_QCAP 2048), 1152-1162 (q_push: `L->dropped++`, source not queued), 1593-1594 (`q_push(...)` then unconditionally `next[i] = now + sched_state_interval(...)`), 1396-1406 and 1516-1519 (boot ramp).
- Scenario: default cadence is global 6 h (cadence.c:22), ~55k scheduled rows. sched_boot_offset() compresses the first run of every row into `JO_SCHED_RAMP_SEC`=900 s, so ~61 rows/s are pushed into a lane that holds 2048 and is drained by 8 workers (the file's own measurement, scheduler.c:1461-1467: 8 workers ~ 63 runs/60 s on a hostgate-bound fleet). The queue is full after ~35 s; every later due row in the burst is dropped, logged as `queue full ... skipping this cycle` (tens of thousands of lines), and its next due time is pushed 6 h out. Because `next[i]` is advanced from the dispatch instant, the phase is preserved, so the same 900 s burst recurs every 6 h and the queue empties in between: workers idle for ~5 h of each 6 while the large majority of rows are dropped again. A row's chance of ever running depends on whether a slot happened to be free in its own second of the burst.
- Why not caught: the drop is counted (`L->dropped`) and printed, but no test/gate runs the pool at registry size; test_sched_pool.c drives a handful of sources. `make registry-floor` and the emit sweeps run rows one by one through the binary, not through the scheduler.
- Magnitude caveat: depends on real run durations (fast feeds ~0.3 s would let 8 workers clear ~25k per burst; the file's own number gives ~5%). The heartbeat line (`queued=`, `busy=`) will show it immediately on a real boot.
- Fix: on a drop do NOT advance next[i] (retry in e.g. 5-30 s, keeping overdue_ratio growing so priority rises); spread the ramp over min(interval, ...) instead of 900 s (`JO_SCHED_RAMP_SEC` should scale with row count), or make the queue unbounded-by-count but bounded-by-memory (it holds an int and a double per entry; 61k entries is 700 KB). Count `dropped` into /api/status.
- Confidence: high on the mechanism, medium on the fraction dropped.

### ENG-02  HIGH  The system-wide 6 h cadence default also retimes the maintenance pods (WAL checkpoint, repair, triage, embedding backfill, enricher, simhash)
- core/cadence.c:107-114 (`apply()` returns `global_sec` for any `declared > 0`), core/scheduler.c:280-292 (`sched_row_interval` has no `_` / pod exemption), 1573/1576. cadence.h:37 documents "a pod that is not scheduled is returned unchanged" - true only for interval 0; scheduled pods are scheduled.
- Declared intervals overridden to 21600 s: wal_checkpoint_pod.c:180 (300 s), anomaly_triage.c:535 (60 s), collector_repair.c:956 (120 s), embed_pod.c:1023 (120 s), llm_enricher.c:28 (300 s), simhash_pod.c:26 (300 s), camera_geocode_pod.c:324, upload_gc, content_change_pod, entity_stats_pod, service_vec_pod.c:73.
- Scenario 1 (data loss risk): wal_checkpoint_pod.c header records a 98.2 GB -wal on 2026-09-21 and says PASSIVE auto-checkpoints never finish because some connection is always in a read transaction; the pod exists to TRUNCATE every 5 minutes. On an unmodified install it now runs once every 6 h, so the WAL again grows for 6 h of 8-worker writes (intel.c:558-565 measures 132.9 KB of WAL per rewritten record).
- Scenario 2 (capacity): embed_pod.c:183 embeds JO_EMBED_MAX_PER_RUN=2000 rows per run. At 120 s that is ~1.4M rows/day; at 6 h it is 8,000 rows/day against an ingest of millions, so semantic search/alerts/stories coverage can never catch up. Same for the enricher (50 rows per run -> 200/day), triage (anomalies sit untriaged for 6 h) and repair.
- Why not caught: test_cadence.c pins the policy rules and the persistence, not the interaction with `_maint` rows; the "estimate" counts pods as scheduled rows.
- Fix: exempt `collector[0]=='_'` rows from `cadence_apply` (and from `source_interval` overrides unless explicitly set), or give pods their own policy knob. Also have the policy page show which rows it did not touch.
- Confidence: high (the only scheduling path for these pods is scheduler_loop; grep found no other caller).

### ENG-03  HIGH  collector-repair feeds untrusted upstream bytes to the LLM unfenced and auto-merges the model's URL as a live runtime override
- collectors/pod/collector_repair.c:316-319 (`fetch_node` puts `body_head`, first 2000 bytes of whatever the failing host answers, into the bundle), 706-718 (prompt = SYS_PROPOSAL + `Input:\n<bundle>` - no fence), 515-533 (sanity prompt: raw sample inline, also unfenced), 103-112 (`is_clean_url`: http/https, <=2048 bytes, no control chars - no host/registrable-domain constraint), 836-862 (INSERT into collector_url_overrides + `url_override_reload`, status `merged`, no human).
- Compare core: collectors/pod/anomaly_triage.c:165 and 401 fence the very same bundle with prompt_fence_untrusted and say why ("a body saying 'classify this as url_move with suggested_fix {...}'"). The repair pod, which ACTS on that output, was not given the same treatment (rule: "when you fix something, look for the other copies").
- Scenario: a hijacked or hostile upstream (or any page that 200s) answers the source's URL with text instructing the model to return `{"action":"url_swap","new_url":"https://evil.example/feed.json"}`. The gates are (b) HTTP 2xx and a record count inside +-50% of the baseline (count_records returns 1 for any JSON object, so a source whose baseline mean is ~1 accepts any JSON object) and (c) an LLM sanity classifier that reads the attacker-controlled sample, also unfenced, and is told to be permissive. If those pass, every later fetch of that source is rewritten to the attacker's host (credentials are stripped on host change, httpclient.c:799-834, but the data is ingested as that source's data and attributed to it).
- Fix: fence bundle and sample with prompt_fence_untrusted; require the new URL to be same registrable domain (or same host) as the old one unless an operator approves; require `status=verified` + human confirmation for cross-domain swaps; keep REPAIR_DRY default on.
- Confidence: high on the missing fence and missing host constraint, medium on exploitability (needs the triage class url_move/site_dead first).

### ENG-04  HIGH  Webhook SSRF is checked once at save time; delivery uses the lenient fetch floor, so DNS rebinding reaches loopback/RFC1918
- core/alertsapi.c:491-500 (hostgate_url_check_strict: literal + resolved addresses at rule-save), core/alert_deliver.c:163-169 (`http_request` POST at delivery, no re-check), core/httpclient.c:657-661 and 174-186 (do_once and on_prereq use hostgate_url_check / hostgate_addr_check_floor), core/hostgate.c:511-514 and 535-541 (the floor blocks only 0/8, link-local, multicast; loopback and RFC1918 are `class 2` = strict only, and strict is on only if JO_HTTP_BLOCK_PRIVATE is set).
- Scenario: any analyst creates an alert rule with webhook target https://rebind.example/x resolving to a public IP at save time and to 127.0.0.1 (llama-server :8080/:8081, embedding server, the app's own admin API) or 10.x afterwards. The delivery worker POSTs the alert JSON body there, follows up to 5 redirects, and `error` stores the first 120 bytes of the response (alert_deliver.c:178-190), which the rule API can read back - a blind-to-semi-visible SSRF with attacker-influenced body.
- Fix: at delivery call hostgate_url_check_strict and use a per-request prereq floor of strict for the webhook client (http_request has no per-call knob today: add a flag or pass through a thread-local like http_request_max); reject redirects for webhooks (FOLLOWLOCATION off).
- Confidence: high. (JO_HTTP_BLOCK_PRIVATE=1 closes it, but it is off by default and documented as off because llama-server lives on 127.0.0.1.)

### ENG-05  MEDIUM  Custom credential headers survive cross-host redirects
- core/httpclient.c:193-194 (FOLLOWLOCATION, MAXREDIRS 5, no CURLOPT_UNRESTRICTED_AUTH handling), 671-676 (caller headers attached as-is). libcurl strips only `Authorization:` (and, since 7.84, `Cookie:`) from a custom header list on a cross-host redirect; `X-API-Key:`, `X-Auth-Token:`, `Api-Key:`, `X-Access-Token:` (the names header_is_credential() lists at :226-244) are sent to the redirect target.
- The tree already knows this class matters: the url_override path strips exactly those names on a host change (httpclient.c:799-834) but the ordinary redirect path does not.
- Scenario: a row with `header1=X-API-Key: ...` whose upstream starts 302-ing to a CDN/other host, or a hijacked host redirecting to a logging endpoint, receives the key.
- Fix: use a per-hop header callback is not available in libcurl; instead set FOLLOWLOCATION off when the request carries a credential header and follow manually after re-checking the host (hostgate_same_host), or use CURLOPT_HEADEROPT=CURLHEADER_SEPARATE with CURLOPT_PROXYHEADER-style split.
- Confidence: medium (depends on libcurl behaviour and on rows actually carrying these headers).

### ENG-06  MEDIUM  The scheduler's fetch-failure notice can report another source's transport/TLS error
- core/scheduler.c:631-692 (`http_last_transport_error()`, `http_last_tls_cause()` read in sched_disclose_fetch_failure), core/httpclient.c:490-498 (`__thread g_last_te`, `g_last_tls`), 646-647 (reset only inside do_once).
- The thread-locals live as long as the worker thread. For a run with `hosts == 0` (no request made: the notice's own outcome text says so) the fields still print whatever the PREVIOUS source on this worker left behind: `"transport_error":"timeout"`, a `tls_cause`/`tls_remedy` for a different host. This is the notice inventing a cause - the exact failure CLAUDE.md rule 1 lists three times.
- Why not caught: test_sched_runline 3-3h pin the shape with a single source per thread.
- Fix: reset both thread-locals (add `http_reset_last_error()`) at the start of scheduler_run_source, and print them only when hosts > 0.
- Confidence: high.

### ENG-07  MEDIUM  A source whose host answers with a refusal page and returns rc<0 reads "online" with records_count=1 and never backs off
- core/scheduler.c:87-96 (`run_status` -> "ok" when every host answered), 842-860 (disclosure emitted AFTER run_st is computed, through the counting sink so `cs.n` becomes 1; it is `collector-fetch-failure`, not a `-notice`), 934-936 (`sources_row_record(..., stored_data, ...)` where stored_data = stored - stored_notices = 1; `sched_state_record(db, d, status, cs.n)` with cs.n=1).
- Result for the il-knesset-dates shape (131,618 bytes of a maintenance page, rc=-1): sources.status='online', last_success moves, records_count=1, consecutive_failures reset, consecutive_empties reset to 0 (so even the EMPTY_RESULTSET counter never grows), no backoff, no quarantine, fetch_log status 'ok'. The disclosure row exists, but every health surface that reads the sources/sched tables says healthy. CLAUDE.md records records_count=1 as "the convention hpengine rows already have" - for hp rows with 404/5xx the run status is an error; this path makes the success-status case look healthy as well.
- Fix: compute `records` for the sources row / sched state from cs.n minus the scheduler's own disclosure (return a flag from sched_disclose_fetch_failure), and let a disclosed failure count as a failure for consecutive_empties/backoff (or add a 'refused' state).
- Confidence: high on the code path, medium on how much anyone relies on those columns.

### ENG-08  MEDIUM  llm_ctx_chars() caches a failed /props read for the life of the process and shares one value across different servers
- core/llm.c:178-218 (`static int cached_ctx = -1`; set to 0 before the request, never reset on failure; one global, not keyed by `c->base_url`).
- Scenario A: first call happens while llama-server is still loading at boot (or the 2 s probe times out) -> cached_ctx=0 forever -> every prompt budget (phase-1 catalogue, results view, etc.) uses compiled defaults even after the server is healthy; log says "stay on their built-in defaults" once and never again.
- Scenario B: the first caller is the interactive/suggest client (LLM_SUGGEST_BASE_URL, small n_ctx) -> that n_ctx becomes the budget for the main 20B analysis server for the whole process.
- Also holds `mu` across a 2 s HTTP call, so concurrent callers queue behind it.
- Fix: cache per base_url, retry unknown after a TTL (30-60 s), do the HTTP outside the mutex.
- Confidence: high.

### ENG-09  MEDIUM  Re-fetched records whose text changed keep their old English translation (and stay marked translated)
- core/intel.c:205-232 (ON CONFLICT DO UPDATE rewrites title/body/summary/properties but never touches title_en, body_en, summary_en, translated_at, translate_failed), core/translate.c:160-161 (a row is pending only when translated_at IS NULL), no other writer resets them (grep of core/*.c: only translate.c writes translated_at).
- Scenario: a Japanese advisory/status/outage record whose title or body changes between fetches (the most common reason to re-emit) is upserted with new Japanese text while title_en/body_en still hold the translation of the OLD text and translated_at is set, so it is never retranslated and the search keyword column (fts_write wipes `keywords`, translate_fts_resync restores the stale English) matches the old content. A reader sees new Japanese beside an English rendering of the previous version, presented as a translation of it.
- Fix: in the DO UPDATE, `translated_at=NULL, translate_failed=0` (and clear *_en) when content_hash changed (the hash is already computed); same for any derived LLM fields (entities extracted by llm-enricher).
- Confidence: medium (the display side was not read).

### ENG-10  MEDIUM  The per-source "http_body_ceiling_bytes" operator setting is a no-op for hpengine and VJSON rows
- core/scheduler.c:798 (resolves it into ctx.body_ceiling), core/source_settings.c (PUT/GET, `effective`), but the only readers of ctx->body_ceiling are collectors/sources/od_ods_geonames.c:94 and trn_france_irve_charging_stations.c (grep for body_ceiling / http_request_max across lib/ and collectors/: lib/hpengine.c and lib/jsonlist.c never read it).
- Result: an operator raising the ceiling on any of the ~44k hp rows or the VJSON fleet gets 200 OK, an audit row and an `effective` value, and the 64 MB cap (httpclient.c:64-104) still aborts the fetch as before. CLAUDE.md itself names this failure ("a capability a comment claims is not a capability anything uses").
- Fix: wrap the engine entry points: in hp_run/jsonlist (or in scheduler_run_source) set the thread-local ceiling around `d->run` via the existing g_body_ceiling_once mechanism, which already clamps to an explicit JO_HTTP_MAX_BODY.
- Confidence: high.

### ENG-11  LOW  A resume token of 1024+ bytes is silently dropped, so a long-URL cycle restarts from page 1 every run
- core/scheduler.c:378-409 (`SCHED_RESUME_MAX 1024`; `sched_resume_store` sets token=NULL and prints one stderr line; sched_resume_load also refuses >= cap).
- hpengine stores the next-page URL as the token (hpengine.c:5533-5550); rows whose URL carries long filters (CQL/ArcGIS `where`, multi-id lists) can exceed 1023 bytes after the page parameter is added. The row's notice says it resumes at that URL, the scheduler discards it, and the row re-reads the same first slice for ever - the stuck-cycle shape audit_cycle_advance.py exists to find, produced here by the scheduler. Disclosure is stderr only.
- Fix: raise to 4096+ (column is TEXT) or return an error to the collector so it files a notice.
- Confidence: medium.

### ENG-12  LOW  Dispatcher backoff lags one cycle; quarantine triggers on failure 13, not 12
- core/scheduler.c:1583-1594 computes `next[i]` from g_state[i] at PUSH time, before the run; the worker updates the persisted state and g_state[i] only after the run (1241-1246, 475-511). So the interval doubling from run N's failure applies to the schedule made for run N+2, and a source that has just tripped quarantine still gets one more normal-cadence run.
- Fix: after the worker refreshes g_state[i], also recompute next[i] (it owns the slot under g_q.mu).
- Confidence: high, small impact.

### ENG-13  LOW  Concurrent per-source interval changes can leave the cache stale
- core/source_interval.c:47-73 (reload reads the table then swaps under the lock), 117-148 (write; reload; generation++). Two HTTP threads on different connections can interleave write A / read-all / write B / read-all / swap B-result / swap A-result, leaving B's override missing from the cache (and the dispatcher) until the next reload or restart, while the DB row exists.
- Fix: hold a dedicated mutex around write+reload, or version the reload by max(updated_at).
- Confidence: medium (needs concurrent operator writes).

### ENG-14  LOW  Missing TLS intermediates bundle fails silent when the path does not resolve
- core/catrust.c:56-63 (fopen failure -> g_n=0, "say nothing"), 9-11 and 18-28 (compiled-in fallback `/Users/rayan/OSINTsaas`, or $JO_REPO_ROOT). A relocated binary/container that sets neither loses the nine-host fix with no log line; the engine then reports `tls_cause: no_local_issuer` for hosts that were fixed. PEM parse stops at the first bad entry (line 69) and silently drops the rest.
- Fix: log once when the file is absent AND any `no_local_issuer` is later seen; log how many certs loaded; continue past a bad PEM block.
- Confidence: medium.

### ENG-15  LOW  Metadata/private ranges the floor does not cover
- core/hostgate.c:511-535: v4 `100.64/10` (includes Alibaba metadata 100.100.100.200) and `192.0.0/24` (Oracle 192.0.0.192) are class 2 = strict only; v6 has no NAT64 (64:ff9b::/96), 6to4 or IPv4-compatible handling, so `::ffff:`-style only is covered. With the default non-strict floor these are reachable from a collector URL, an LLM-proposed override (see ENG-03) or an alert webhook (ENG-04).
- Fix: treat 100.100.100.200, 192.0.0.192 and the embedded-IPv4 v6 forms as floor (class 1).
- Confidence: medium.

### ENG-16  LOW  Non-thread-safe static error buffers on the HTTP path
- core/alertsapi.c:462-464, 472-474 `static char eb[64]` / `gb[128]` returned to the caller and rewritten by every request; concurrent rule-create calls on off-loop workers can interleave and return a garbled or truncated validation message (and `eb[64]` truncates `invalid channel type: %s`).
- Fix: return a copy or a table of constant strings.
- Confidence: medium (only if that handler runs on more than one thread).

Rejected after reading (looked like bugs, are not): intel.c uid truncation (make_uid hashes long keys), ffmpeg spawn/reap and fd cloexec, llm_embed index/dim/NaN validation, llm_worker queue (withdraw vs started is correct under w->mu), progress eviction (snapshot done under the lock), hostgate slot reclaim, retention_pod fetch_log window delete, thresholds cache, trust cache rebuild thread, alert email subject injection (fixed in the code).

---------------------------------------------------------------------------
## Design weaknesses / risky structure (not bugs)

1. One SQLite writer for the whole fleet, with a heavy per-record write. emit() (intel.c:502-679) is one BEGIN..COMMIT per record containing the upsert (10+ indexes), an FTS5 delete+insert and uid_map upsert, then a second transaction for simhash (4 band upserts + row update, simhash.c header) and alert evaluation. intel.c:558-565 records 132.9 KB of WAL per rewritten record (2.66 GB per 20k). The cadence default is 220,696 runs/day (cadence.c:17-21); at even 100 changed records/run that is ~22M records/day, i.e. a WAL-write budget measured in TB/day through a single writer lock shared by 8 collector workers + 2 maint workers + the HTTP loop + the alert worker. Batching records per run in one transaction (the sink already has the run boundary) is the missing lever; the "unchanged" path (touch only last_seen_at) is the only batching-like optimisation.
2. The scheduler is not host-aware; workers block inside hostgate_acquire. q_pop (scheduler.c:1174-1193) picks by trust/overdue only. Override hosts wait up to 300 s (hostgate.c:208, 349-352): news.google.com has 615 registered rows at a 1.5 s gap (hostgate.c:104-108); batch families register contiguously, and the ramp compresses 615 contiguous rows into a ~10 s window, so all 8 workers can park on one host for ~15 minutes while the rest of the queue waits. reddit.com (30 s gap) can pin 8 workers for 4 minutes. Fix: per-host in-flight accounting in the dispatcher, or skip a queue entry whose host is currently gated and take the next.
3. Cancellation is cooperative and the watchdog sets the flag only at max(deadline, interval) (scheduler.c:1312-1315). With the 6 h global cadence a source that never polls `ctx->cancel` (most VJSON/bespoke ones) holds a worker slot up to its own HTTP timeouts and is "cancelled" only after 6 h; a handful of long page walks (data.qld.gov.au is documented at ~72 minutes) consume a meaningful share of an 8-worker pool.
4. A global 6 h default demotes every freshness-critical row (60 s EEW/ADS-B feeds, the 12 sources scheduler.c:985-1000 cites) unless an operator sets a per-source override; no row can say "I am a real-time source, exempt me from the policy" (cadence.c:107-114). Combined with ENG-02 this is an owner decision with side effects the policy page does not state.
5. LLM routing menu is a prefix of the registry. osint_services_list_bounded (osint_dispatch.c:421-475, 514-530) keeps the first K ids in link order; the file itself measures that hand-written pivots (DNS_RECORDS, DOMAIN_WHOIS, JP_CORPUS_LOOKUP) sit at the tail and are the first dropped, while the banner tells the model dropped services "still exist and can be reached" even though the schema enum restricts it to the shown ids. Routing quality on a 61k-source registry depends on the semantic path (service_vec), which silently degrades to this prefix when the embedding server is down.
6. No per-search work bound. run_tasks (pipeline.c:440-499) fans out `entities x recommended_services` with no cap other than the model's 2048-token reply and fanout 6-16; each task can be a multi-page walk. One query can generate tens of outbound walks and fetch_log rows (and anomaly inputs, see dispatch_impl:994-998).
7. Per-record side effects on the ingest path: alert_eval_on_item runs an FTS probe per rule per new row (alert_eval.c:961-975, 1036), unbounded by rule count; simhash recomputes MeCab segmentation on every changed record. Fine at today's rule counts, a hazard when tenants add rules.
8. Dispatcher is O(registry) every second under two locks: scheduler.c:1564-1595 walks all 61k rows each tick, watchdog_scan (1317-1342) and the 60 s heartbeat (1607-1611) walk them again under g_q.mu. Cheap in absolute terms (tens of microseconds per 61k simple loads; not measured here) but it is the reason a timer wheel / min-heap by next_due would remove an O(n) cost per second.
9. Alert delivery is single-threaded and serial (alert_deliver.c:674-704, drain_once): one slow webhook (WEBHOOK_TIMEOUT_MS) blocks every tenant's deliveries; delivery is at-least-once (crash between deliver_one and finish_delivery re-sends). The X-JO-Delivery header makes receivers able to dedupe; nothing documents that to rule authors.
10. Shared mutable global state keyed by "first caller wins": llm_ctx_chars cache (ENG-08), vecindex_live_model uses JO_EMBED_MODEL as the *identity* when set (vecindex.c:63-67), which defeats the same-width-swap refusal the rest of vecindex_live_* implements - an operator who pins the env var gets no protection against the server actually serving another model.
