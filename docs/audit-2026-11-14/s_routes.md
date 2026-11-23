# s_routes report

Branch `worktree-agent-af5b68df699bb4589`, commit c163abe (on top of WIP 097d7cc and a clean merge of `claude/peaceful-ritchie-i7e0hj` tip 2f2e3ba; the worktree was first reset to the audit branch 2747d7e because it started from main without the audit fixes). All verification ran on the merged tree.

## Design
- `native/core/routes.{c,h}`: one static table, 140 rows: method set (RM_*, RM_ANY), pattern (exact, `*` prefix, `:param` single segment, `:param/literal-tail` with seg() semantics), gate, runs_on (LOOP/WORKER), name, optional deny body. First match wins; RF_STRICT rows answer 405 instead of falling through to a broader prefix row.
- Gates: PUBLIC 3, AUTH 39, TENANT 43, ROLE_ANALYST 3, ROLE_ADMIN 0 (defined, unused), RUN 2 (operator, or owner/admin of a team workspace), OPERATOR 50.
- httpd.c `fn()`: looks the request up before the chain; the pre-auth handlers (health, search stream, break-glass login) are reachable only through a PUBLIC row. After the token check, no row gives 404 and a method miss gives 405, both before the chain. Then `route_gate_enforce()` is the single place gates are enforced, with the same JSON error shapes as before (401 Auth required, 403 operator triad, not_a_member, forbidden). 401 for unauthenticated callers is unchanged.
- `GET /api/admin/routes` (operator): `{data:[{name,method,path,gate,runs_on}], meta:{total,by_gate}}`.
- tenantapi.c: per-request memo of `tenant_resolve` (loop thread only, begun per request) so central plus handler resolution costs one resolve; `tenants.personal` column added lazily and set on auto-provisioning, backfilled by the "'s workspace" name; `tenant_is_personal()` = flag set and at most one member, fail-closed.

## Per-handler checks
Removed (central gate identical): cadence PUT, interval PUT/DELETE, schedule PUT, settings PUT (row carries the `operator_required` body), metrics, status probe/consent, follow/recent, the whole /api/admin + /api/db block opener, breach/search, evidence/gc, evidence/verify (row carries `forbidden` body), media and camera-stills capabilities, platform /api/keys (both checks), alerts/preview viewer check.
Kept (finer than the row): breach_gate on breach uids/reveal/export, GET tenant-keys/:name operator check (sub-path variants), keys/tenant role checks, collector_run_gate (pod rule, rate limit, entity validation) in /run, cameras/trigger, /api/data cold run, uploadapi's role check, entity breaches operator check, evidence/stills raw operator checks, all tenant_resolve calls in handlers (memoised).

## API items
- API-08: per-user in-flight cap on off-loop workers: 4 (`JO_HTTP_USER_WORKERS`, 0 disables), 429 `too_many_requests_in_flight` with Retry-After. `worker_admit()` charges `g_req_uid` (set from the verified token); all 14 `pthread_create` sites and `XS_THREAD_CREATE` now go through `worker_spawn()`, whose trampoline hands the slot to a thread-local that `worker_release()` reads; a failed spawn releases on the loop thread.
- API-07: upload begin and part now run on a worker with its own db handle (same path as commit); status/abort stay inline.
- API-02: collector-run gate now requires operator, or owner/admin of a non-personal workspace (pods remain operator-only). Applies to /run, cameras/trigger, cold /api/data.
- Unpaged dumps: /api/status, /api/intel/sources default to 500 rows (explicit limit clamped to 500, summary=1 unchanged); /api/sources returns `{data,page}` bounded 500. Operator `?all=1` restores the unbounded form (/api/sources then returns the old bare array). `page{limit,offset,total,has_more}` added to status and intel/sources envelopes (existing view/meta kept).
- Clients: web client uses only `/api/status?summary=1`, `/api/status/:id`, `/api/sources/search`; iOS uses summary, a limit-500 window, and per-id rows. Nothing calls the unbounded forms. Remaining callers: tests/contract/run.sh (compares against May 2026 Node baselines and already DIFFs; not run in CI) - I did not touch it.
- API-11, API-17 and design item 6 (414 on over-long ids) were not touched: outside this brief.

## Tests
- `JO_UNIT_ONLY='test_(routes|collector_run_gate|tenant_resolve|export_stream|cors|near_q)$' tests/unit/run.sh`: all passed (test_routes checked 149 chain matchers parsed from httpd.c; asserts admin/db rows OPERATOR, pre-auth set exactly health, search stream, break-glass login, source writes OPERATOR vs reads AUTH, 405/404 semantics).
- `make authtest`: all passed. `PORT=4177 tools/ci_concurrency_gate.sh ./bin/japanosint 1.0`: ok (worst health 0.049 s).
- `make -j2`: full build, 0 compiler warnings.
- Live smoke (`scratchpad/smoke.sh`): /api/health 200 unauthenticated; /api/me 401 without token; unknown /api/nope 404; /api/admin/routes 403 for a user and 200 with route list for an operator; PUT cadence 403; POST run by a personal-workspace owner 403 with the personal-workspace message; /api/sources?limit=2 and /api/intel/sources answer bounded.
- The full unit suite was not run (only the tests that include or exercise the edited files). A new test for tenant_is_personal / the personal run gate and the per-user cap was not written.

## Behaviour changes clients will see
- Unknown /api/ paths: 404 `not_found` (previously the chain's 404, or 403 for unknown /api/admin/ paths for non-operators).
- Strict-method rows (isochrone, vocab, timeline, export, docmeta, evidence/gc, ...) return 405 from the table with the same body the handlers used.
- Personal-workspace owners can no longer run collectors or trigger cameras (403).
- More than 4 concurrent off-loop requests from one user get 429 (map clients loading many /api/data layers at once may hit it; tune `JO_HTTP_USER_WORKERS`).
- /api/status and /api/intel/sources return at most 500 rows unless an operator passes `all=1`; /api/sources shape is `{data,page}` for non-`all` callers.
- A tenant-keys GET on a sub-path or a rejected X-Tenant-Id now fail with the first applicable gate (operator before not_a_member for platform keys); statuses are unchanged.
