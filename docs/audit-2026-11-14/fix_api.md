# fix_api report

All touched files pass `cc -fsyntax-only -Wall -Wextra` with zero warnings. Nothing was built or run.

## Contracts the web/iOS agents rely on
POST /api/intel/sources/:id/run
- Optional JSON body `{"entity": "<string>"}` (trimmed, under 512 bytes, whole body at most 4096).
- Pivot (registry_is_entity_pivot) with no entity: HTTP 422 `{"error":"needs_entity","source_id":...,"detail":...}`. The collector is not run.
- Entity given for a non-pivot or a pod: HTTP 400 `entity_not_accepted`.
- Entity too long: 400 `entity_too_long`. Non-string entity: 400 `entity_must_be_a_string`. Bad JSON: 400 `invalid_json_body`. Body over 4096 bytes: 413.
- Success response shape is unchanged (`ran, source_id, ingested, duration_ms, kind, meta`).
- Gate: platform operator, OR workspace owner/admin with 10 runs/min.
- Pods: operator only, 403 for everyone else. The entity-less operator run of a pod is still allowed (deviation from "a pod is refused"; the Admin re-run button may need it). Say if you want pods refused outright.

GET /api/sources/:id/detail adds `layer_id` (string or null, resolved via layertab) and `records_geocoded` (count of items with lat NOT NULL). Both are at the top level of the detail object AND inside `registry`; existing fields are untouched.

GET /api/collectors?q= is a case-insensitive substring filter on key, label and label_ja. `meta.total` counts rows shown; `meta.collectors_total` is the unfiltered count when q is set. `groups` stays the full fold.

## Per finding
- API-01 FIXED. casesapi.c build_snapshot uses intelapi_item_by_uid_tenant. A `breach:` intel_item resolves for operators only; a client snapshot is ignored for breach refs. New `casesapi_ex(..., is_operator)`; `casesapi()` is the wrapper with 0; httpd passes opgate_check()==0.
- API-02 PARTLY. GET /api/data/<id> refuses pods with 403 and runs a collector only after the same gate as /run (collector_run_gate) plus run_begin/run_end single-flight. Both happen only on a cache miss, evaluated on the worker's own db handle. A personal workspace cannot be told from a real one (no marker column on tenants), so pods are operator-only everywhere and the owner/admin gate for ordinary collectors is unchanged. Side effect: a non-owner/admin in a team workspace now gets 403 on a cold layer instead of triggering a run.
- API-12 FIXED. cameras/trigger uses the same gate.
- WEB-01/IOS-05 server half FIXED (see contract above).
- API-04 FIXED. Invites match the token's own email claim and require it verified; users.email is refreshed from the token on login. Verified addresses are recorded in a lazily created table `user_verified_emails` (the schema file is not mine).
- API-05 FIXED. "Invite existing user" adds immediately only for an account that has proven the address; otherwise the invite is queued.
- API-06 FIXED. Changing or removing an owner membership needs an owner (or the member themself).
- API-13 FIXED. A foreign X-Tenant-Id makes tenant_resolve return -403. httpd's 24 call sites map it via `reply_tenant_err` to 403 `not_a_member`. An absent header keeps the fallback.
- API-07 PARTLY. Off-loop via sem_spawn / worker threads:
  - layer geojson
  - /api/transit*
  - sort=relevance|trust with q on /api/intel/items and /api/intel/search
  - POST /api/docmeta
  - POST /api/uploads/:id/commit (own db connection)
  Not moved: the upload begin/part/status/abort handlers. They are short and use the shared handle's transactions, so moving them is not contained. No query deadline was added, as instructed.
- API-08 PARTLY. `?refresh=1` on summary is ignored for non-operators. The per-user in-flight cap was not added: worker_admit() carries no user id and worker_release() is called from about 25 threads.
- API-09 FIXED. CSV text cells starting with = + - @ TAB or CR get a leading `'`. Numeric cells and the header row are left alone.
- API-10 FIXED. The overlay is created 0600, fsynced and checked, and PUT returns 500 on failure. The b64 accumulator is unsigned.
- API-11 PARTLY. Replay of an already accepted TOTP step is refused. The proxy/XFF limiter design is untouched, as instructed.
- API-14 FIXED. Search query over 16 KiB gets 413. More than 64 open uploads per tenant gets 429 `too_many_open_uploads`. More than 500 alert rules per tenant gets 409.
- API-15 FIXED. A token without `aud` is rejected (the audience defaults to "authenticated"). No fixtures needed changing; the break-glass path returns earlier.
- API-17 PARTLY. image/svg+xml gets 415. No camera-row predicate exists, so the uid restriction was not added.
- ENG-16 FIXED. alertsapi validate_rule uses caller buffers instead of statics.
- WEB-12 no server change.
- WEB-09, WEB-08 FIXED server half (above).
- OPS-14 FIXED. New core/metricsapi.{c,h} and `GET /api/metrics` (operator gate, text/plain) with these gauges:
  - japanosint_sources_registered, _scheduled, _pivots, _pods
  - japanosint_collector_runs_1h, japanosint_collector_failures_1h (via idx_log_ts)
  Scheduler queue depth and intel_items count are omitted: not exposed or not cheap.

## Tests
- tests/unit/test_case_roles.c: API-01 section added.
- tests/unit/test_tenant_resolve.c (new): API-04, API-05, API-06, API-13.
- tests/unit/test_auth_sec.c: break-glass replay and no-`aud` cases.
- tests/unit/test_export_csv.c (new): API-09.

## To verify
- Run `make unit`. The new tests are unrun; test_tenant_resolve mints HS256 tokens through auth_check with only SUPABASE_JWT_SECRET set.
- test_auth_sec: the replay assertion comes right after the first successful break-glass login.
- Check the 204 path of the upload commit worker, which uses wakeup_reply with an empty body.
- Check that the Admin UI does not rely on operators running pods through /run with an entity.
- I did not edit http_request_strict, scheduler.c, httpclient.c, hostgate.c or alert_deliver.c. API-03 and API-16 need that other agent.
