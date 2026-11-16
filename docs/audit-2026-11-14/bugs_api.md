# API / HTTP / security surface: bug hunt (native/core), read-only

Scope read: httpd.c (all routing, gates, wakeup/parking, CORS), auth.c, operatorgate.c, tenantapi.c, keysapi.c,
cors.c, ratelimit.c, hostgate.c, cameraproxy.c, alert_deliver.c, alertsapi.c, uploadapi.c, exportapi.c (plan/CSV),
reportapi.c (escaping), casesapi.c (pin path), intelapi.c (where/tenant), dataapi.c, netjail.c (resolve/pin),
sourceapi*.c, thresholds.c, evidence_raw. Things verified CLEAN are listed at the end so nobody re-audits them.

Severity scale: critical / high / medium / low. Confidence = confidence the defect is real as described.

---------------------------------------------------------------------------------------------------------------

## API-01  HIGH  Case pin of `breach:<id>` through ref_type=intel_item bypasses breach_gate and the tenant filter
- File: native/core/casesapi.c:431-434 (build_snapshot), native/core/intelapi.c:194-229
- Trigger: any analyst (or case lead/contributor) sends
  `POST /api/cases/:id/items {"ref_type":"intel_item","ref_id":"breach:<keyid>"}` then `GET /api/cases/:id/items`
  (or the report export). build_snapshot() calls `intelapi_item_by_uid(db, ref_id)`, which is
  `intelapi_item_by_uid_tenant(db, uid, NULL)`; its first statement reroutes any `breach:` uid to
  `breach_adapter_item_by_uid()`, which returns the breached IDENTIFIER in cleartext. The result is persisted in
  `case_items.snapshot_json` and served to every case reader.
- Why checks do not stop it: the `breach_item` branch two lines below has an explicit comment ("NO server-side
  fetch for breach rows ... a gate on one of N doors is not a gate") but the `intel_item` branch reaches the very same
  adapter. build_snapshot has a tenant_ctx but no auth_user, so breach_gate/opgate_check cannot run here. The
  unscoped variant also means any `uid=` of another workspace's private row (e.g. `osint-search|run:<request_id>`,
  which share links carry) is copied into the caller's case, defeating the tenant-scoped /api/search/results.
- Fix: call `intelapi_item_by_uid_tenant(db, ref_id, t->tenant_id)` and refuse/skip `breach:` uids unless the
  caller is a platform operator (pass the operator flag into build_snapshot, as entityapi does). Also reject client
  `snapshot_json` for breach refs (it is accepted verbatim at casesapi.c:411-413 for any ref_type).
- Confidence: high.

## API-02  MEDIUM  GET /api/data/<id> lets any authenticated user (even workspace `viewer`) run any registered collector or maintenance pod
- File: native/core/httpd.c:5071-5107 -> dataapi.c dataapi_layer_admit() (def->run at ~line 800)
- Trigger: `GET /api/data/log-retention`, `/api/data/collector-repair`, `/api/data/llm-enricher`,
  `/api/data/breach-corpus`, `/api/data/upload-gc`, `/api/data/geojson-file`, or any scheduled collector with
  upstream API keys. No role check, no `_`-lane exclusion, no single-flight (`run_begin`) so it also runs
  concurrently with the scheduler copy. Only throttle: 60 runs / 5 min per user (httpd.c:1914).
- Why checks do not stop it: POST /api/intel/sources/:id/run (httpd.c:2993-3025) is documented as "platform operator
  or workspace owner/admin", but tenant_resolve auto-provisions a personal workspace with role `owner` to every
  user (tenantapi.c:190-209), so that gate is also trivially satisfied by anyone. /api/data/* has no gate at all.
- Impact: pods write the shared DB directly (retention prune, WAL checkpoint, repair proposals); platform API keys
  (ctx.tenant==NULL falls back to platform credentials) and LLM budget are spent on behalf of any signed-in user.
- Fix: refuse `collector` names starting with `_` (and pods) on both routes; require operator for pods; call
  run_begin() in datarun; make the "owner/admin" gate about a non-personal workspace or require operator.
- Confidence: high on reachability, medium on blast radius (depends on which pods do real work).

## API-03  MEDIUM  Alert webhook SSRF: delivery re-checks only the "floor" (loopback/RFC1918 allowed) and follows redirects
- File: native/core/alert_deliver.c:146-170, native/core/httpclient.c:174-210, native/core/alertsapi.c:482-500
- Trigger: analyst creates a webhook rule whose public URL passes `hostgate_url_check_strict` (save-time). At delivery
  the URL (a) can 302/307 to `http://127.0.0.1:11434/...` or `http://10.x/...` or (b) is a name that re-resolves to a
  private IP (DNS rebinding after the save-time lookup). http_request() uses FOLLOWLOCATION=1, MAXREDIRS=5 and the
  prereq hook `hostgate_addr_check_floor()`, which only blocks link-local/metadata unless JO_HTTP_BLOCK_PRIVATE=1
  (off by default; llama-server lives on 127.0.0.1). For a non-2xx answer the first 120 bytes of the response body are
  stored in alert_deliveries.error and returned by GET /api/alert-events/:id/deliveries: a partial read-back.
- Fix: deliver webhooks through a strict path: use `hostgate_camera_addr_check`-style strict prereq (or netjail's
  pinned resolver) and disable redirects for webhooks; do not echo response bodies.
- Confidence: high for the code path; medium for exploitability (depends on JO_HTTP_BLOCK_PRIVATE).

## API-04  MEDIUM  Invite claim matches the STORED users.email but trusts the CURRENT token's email_verified flag
- File: native/core/tenantapi.c:67-160 (esp. 124-128), users insert at 82-88 (never updated)
- Trigger: attacker signs up as victim@corp (IdP with confirmation off / pre-confirm sessions). The first request stores
  that unverified address in `users.email` (no later UPDATE exists anywhere). Attacker then changes their IdP email to an
  address they own and verifies it: the token now carries `email_verified:true` for the NEW address, but
  tenant_resolve() queries `tenant_invites WHERE lower(email)=lower(<stored victim address>)`, joins the victim's workspace
  with the invited role (can be owner/admin) and deletes the invite.
- Fix: use the token's own `email` claim (auth_user.email) for the invite match, and require the verified claim to be
  for that same string; refresh users.email on login.
- Confidence: medium (needs an IdP that lets an unverified signup or an email change; the DB-vs-token mismatch is certain).

## API-05  MEDIUM  Admin "invite existing user" adds whichever account claims that email, verified or not
- File: native/core/tenantapi.c:569-587
- Trigger: attacker pre-registers victim@corp (unverified, stored in users.email; email is not UNIQUE). A workspace admin
  later invites victim@corp: `SELECT id FROM users WHERE lower(email)=?1` (no ORDER BY / LIMIT / verification) picks the
  squatter and inserts the membership immediately, with no accept step.
- Fix: only add immediately when the account's email is verified; otherwise always queue the invite (claimed on verified
  sign-in, API-04 fixed).  Confidence: medium.

## API-06  MEDIUM  A workspace `admin` can demote or remove owners (up to the last one)
- File: native/core/tenantapi.c:645-700 (PATCH :id/role, DELETE :id)
- Trigger: admin calls `PATCH /api/members/<ownerId>/role {"role":"viewer"}` or `DELETE /api/members/<ownerId>` while
  >=2 owners exist. may_grant_role() only restricts GRANTING owner; the last-owner guard only keeps one owner. The
  comment above may_grant_role says the intent is that owner is a real boundary (owner-only key policy).
- Fix: owner-or-self required to alter an existing owner membership.  Confidence: high.

## API-07  MEDIUM  Synchronous heavy work on the single mongoose event loop (no query deadline anywhere)
No `sqlite3_progress_handler`/`sqlite3_interrupt` exists in the tree. Inline-on-loop routes reachable by any user:
- `GET /api/layers/:id/geojson` (httpd.c:2943-2952 -> dataapi.c:434-451): COUNT(*) over a layer predicate on a multi-
  million-row table plus up to 50,000 features with per-row JSON parsing, all inline.
- `/api/transit*` (httpd.c:5111-5139): trip x stop_times x shapes interpolation inline (rate-limited sibling
  /api/isochrone is off-loop and limited, this is not).
- `/api/intel/items?q=...&sort=relevance` and `/api/intel/search` (httpd.c:2766-2785): bm25 over all FTS hits sorted
  inline; JO_TOTAL_CAP only bounds the count, not the ranked scan.
- `POST /api/docmeta` (httpd.c:4366-4390), `/api/uploads/*` (httpd.c:4397-4428): part write does open/write/fsync/rename +
  BEGIN IMMEDIATE on the loop; `commit` assembles up to JO_UPLOAD_MAX_BYTES (default 64 MiB, clamp 256 MiB) into malloc,
  sha256s it, runs docmeta_extract and evidence_capture (file write) inline.
- `tenant_resolve()` and audit_write() do SQL writes on the shared handle with busy_timeout=5000: a WAL checkpoint /
  scheduler writer stalls the loop for up to 5 s per request.
- Fix: route these through worker_admit()/sem_spawn like their siblings; add a progress-handler deadline for loop-side
  queries.  Confidence: high that they are inline; medium on how long they take (data dependent).

## API-08  MEDIUM  No per-user/tenant fairness on the shared worker pool, LLM and search slots
- Files: httpd.c:920-980 (worker cap = 16 global), httpd.c:3588-3625 (geocode: up to 24 s, no rate limit, public
  Nominatim egress on the platform IP), searchapi.c:51-70 (4 global search slots),
  sourceapi_summary.c:318 + httpd.c:259-306 (`?refresh=1` on /api/sources/:id/summary forces an LLM regeneration every
  call for any signed-in user).
- Trigger: one user keeps 16 slow requests in flight (/api/geocode, /api/data/<cold layer>, summary?refresh=1) and every
  other user's off-loop route answers 503 server_busy; 4 /api/search/analyze calls monopolise all research slots.
- Fix: per-user in-flight caps and rate limits (the isochrone route already shows the pattern); drop `refresh` for
  non-operators.  Confidence: high.

## API-09  LOW-MEDIUM  CSV export is vulnerable to spreadsheet formula injection
- File: native/core/exportapi.c:674-710 (csv_needs_quote/csv_field)
- Trigger: any scraped feed item whose title/summary/properties begin with `=`, `+`, `-`, `@`, tab or CR (attacker controls
  upstream content) is exported verbatim; opened in Excel/LibreOffice it executes. Quoting is RFC 4180 only.
- Fix: prefix such cells with `'` (or a tab) at emit time for text columns.  Confidence: high.

## API-10  LOW-MEDIUM  Platform key overlay `data/api-keys.json` is written with default umask and write errors are swallowed
- File: native/core/keysapi.c:111-120 (overlay_write), callers at :252-256 (PUT /api/keys/:name)
- Trigger: `fopen(tmp,"wb")` creates the file 0666&~umask (typically 0644) holding PLAINTEXT third-party API keys; rename()
  failure / disk full is ignored and the handler still answers 200 with `set:true`. Also JO_REPO_ROOT defaults to a
  hard-coded `/Users/rayan/OSINTsaas` path when not defined at build. b64_decode() (keysapi.c:63-78) uses a signed int
  shift accumulator (the same UB auth.c already fixed).
- Fix: open with mode 0600 (fchmod), fsync, check return of fwrite/rename and return 500; unsigned accumulator.
  Confidence: high.

## API-11  LOW-MEDIUM  Break-glass login: TOTP replay, and the per-IP limiter degrades behind a proxy / under table pressure
- Files: keysapi.c:560-585, httpd.c:2281-2288, ratelimit.c:45-76
- (a) A valid 6-digit code is accepted repeatedly across the +-1 step window (no last-used-step tracking), so one shoulder-
  surfed/logged code is a 90 s reusable operator login.
- (b) The key is `c->rem` (peer IP). The documented deployment is "behind a TLS-terminating proxy", so every client shares one IP: 5
  wrong attempts/min from anyone locks the emergency path for everyone, and X-Forwarded-For is never consulted.
- (c) RL_BREAKGLASS is fail-closed with 512 slots: ~512 distinct source IPs (trivial with IPv6) pin the whole table for 60 s
  and every break-glass attempt is refused ("fail closed" turns into a DoS on the emergency path).
- Fix: store last accepted step; key the limiter on a trusted forwarded IP (config) plus a global ceiling; bucket IPv6 by /64.
  Confidence: medium-high.

## API-12  LOW  POST /api/data/cameras/trigger is plain-auth and fans out 16 scrapers
- File: httpd.c:4982-5058. Only a global cooldown (CAM_COOLDOWN_SEC) and single-flight protect it; any signed-in user can fire it
  repeatedly, burning platform egress/quota and one of the 16 worker slots for the whole fan-out. Gate like /run.  Confidence: high.

## API-13  LOW  Unknown/foreign X-Tenant-Id silently falls back to the user's first workspace
- File: tenantapi.c:174-214. The header is matched against memberships; no match -> `first_tid`, no 403. A client with a stale or
  mistyped tenant id writes (cases, alerts, uploads, keys) into a different workspace than it believes. Return 403 for a
  non-member id.  Confidence: high.

## API-14  LOW  Unbounded growth primitives for analysts
- uploadapi.c h_begin (:517-620): no cap on open uploads or total scratch bytes per tenant (each declares up to 64 MiB, TTL days).
- searchapi.c searchapi_analyze (:130-200) and `owner_persist`: query length unbounded up to the 2.5 MiB body, persisted in
  search_run_owners.query and fed to LLM/collector fan-out (search_history is capped at 16 KiB, this is not).
- alertsapi.c POST create: no per-tenant rule cap; every ingest evaluates every rule.  Confidence: medium.

## API-15  LOW  JWT `aud` is optional
- File: auth.c:602-617. `if (g_aud && aud && !IsNull(aud))`: a correctly signed token with NO `aud` skips the audience check
  (jose with an audience option rejects it). `iss` is checked when known, so impact is limited to same-issuer tokens minted for
  another audience (service tokens).  Also `b64url_decode` ignores invalid characters (signature malleability, harmless today).
  Confidence: high on behaviour, low on impact.

## API-16  LOW  Media pod fetches item image URLs at floor strength
- File: media.c:2030-2040 (http_request on `media_assets.url`, urls come from scraped upstream items). Camera fetches were made
  strict-always; this path allows loopback/RFC1918 under the default floor, so a feed item can make the server request internal
  HTTP endpoints (blind SSRF; only image-sniffing responses are processed, EXIF/OCR text stored).  Confidence: medium.

## API-17  LOW  Camera proxy serves any intel_items uid and echoes `image/svg+xml`
- File: cameraproxy.c:177-206 (`strchr(uid,'|')` => any uid) and :250-262 (only prefix "image/" checked). Any row with a
  `url`/`image` property can be proxied (open image fetcher for public URLs); SVG is returned from the API origin without
  `Content-Security-Policy: sandbox`. Mitigated by bearer-header auth (not navigable) and nosniff.  Confidence: medium.

---------------------------------------------------------------------------------------------------------------

## Design weaknesses / risky structure (not individually exploitable bugs)
1. Auth, tenancy and role checks are copy-pasted at ~60 call sites in a 5,245-line if-chain (`X-Tenant-Id` parse + tenant_resolve
   repeated verbatim). The defects above (API-01/02) are all "one door of N forgot the gate". A route table with a declared gate
   (`PUBLIC|AUTH|TENANT|ROLE>=analyst|OPERATOR`) enforced centrally would make that class of bug unrepresentable.
2. Tenancy is opt-in per query (`tenant_id IN (?, 'legacy')`) and `intelapi_item_by_uid()` is still exported as an UNSCOPED helper
   (alias of the _tenant variant with NULL). Make the unscoped one internal-only / rename it `_unscoped`.
3. "Operator gate" for collector runs is defeated by auto-provisioned personal workspaces where every user is `owner`; role checks
   that depend on workspace role mean nothing for platform-level resources.
4. Single shared sqlite handle for the event loop with explicit BEGIN IMMEDIATE/COMMIT in request handlers (uploads, alerts, aoi,
   cases); a failure mid-way leaves a transaction open for every later request (the code has several comments about exactly this).
   Handlers should take a per-request connection or use SAVEPOINT with guaranteed cleanup.
5. Parked-reply table WBODY_SLOTS=16 == worker cap: correctness of "no lost reply" relies on two numbers staying equal; JO_HTTP_MAX_WORKERS
   can break it (logged only). Wakeups to an already-closed connection leave the parked body until evicted.
6. Path segment buffers are fixed (127/255 bytes). `mg_url_decode` failure leaves the id EMPTY and the handler then runs
   the collection semantics (GET list / POST create / POST /alert-events read-all) instead of 414. seg()/memcpy truncation at
   `rest[256]` is the same shape. Refuse over-long ids with 414.
7. SSRF defence depends on libcurl's PREREQ hook seeing the destination address. Behind an HTTP(S) egress proxy (HTTP(S)_PROXY env,
   as in this very container) `primary_ip` is the proxy, so floor/camera checks stop meaning anything; document/force `CURLOPT_PROXY=""`
   for gated fetches or check the host on the proxy path.
8. Secrets handling: GET /api/tenant-keys/:name returns the decrypted key (operator-gated, audited) and the platform overlay is plaintext
   on disk; HKDF salt is the tenant id (fine) but there is no key-id/rotation field on tenant_secrets blobs.
9. Search stream key travels in the query string (`?key=`): lands in proxy/access logs; the owner-tenant Authorization alternative exists
   and should be the default.
10. Rate limiting exists for four routes only (break-glass, JWKS, isochrone, collector run). Everything expensive (geocode, summary refresh,
    export, report, alerts/preview, semantic search with embedding calls, uploads) is unthrottled apart from the global worker cap.

---------------------------------------------------------------------------------------------------------------

## Verified clean (read end to end, no defect found)
- Body size: Content-Length precheck + MG_EV_READ belt (httpd.c:2128-2152) rejects oversize; chunked covered by recv.len.
- SQL: every dynamic WHERE in intelapi/nearapi/semsearchapi/timelineapi/exportapi builds placeholders only; identifiers come from
  literal tables (exportapi `cases_columns` uses PRAGMA names with a deny-list and 64-byte bound; dbexplorer has a table allow-list and
  column-name validation). No injection found.
- Path traversal: uploads use id_is_uuid() + shard dirs, blob paths from sha256; breach ingest/preview confined via realpath.
- Header splitting: Content-Disposition names come from literal tables or validated ids (evidence_raw/camera_stills_raw look the id up first).
- Report HTML/MD escaping (reportapi.c d_text/safe_url) is complete.
- JWT: HS256 CRYPTO_memcmp, alg pinned, exp mandatory, nbf, iss, aud array; JWKS refresher thread off-loop with global refetch budget;
  break-glass claims verified; `email_verified` thread-local reset per request.
- SMTP header injection in alert_deliver encode_subject is closed; webhook save-time strict check present.
- netjail.c resolves once, judges every answer, and connects to the judged address (pinned) for ffmpeg.
- ffmpeg/exiftool spawn via argv arrays (no shell).
- Entity graph max_nodes is capped at 2000 (entityapi.c:1047).
- CORS: exact-origin allow-list, wildcards/null refused, preflight before auth; Authorization bearer only so no CSRF exposure.
