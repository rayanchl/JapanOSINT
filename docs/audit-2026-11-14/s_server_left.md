# s_server_left: remaining server-side audit items

Branch: worktree branch (reset to claude/peaceful-ritchie-i7e0hj a76024a first). Not pushed.

## Per id

### API-07 FIXED (uploads); rest was already off-loop
HEAD already had begin, part and commit on a worker (`upl_thread`, own db handle). What stayed inline was
status (GET) and abort (DELETE): both touch the part rows and abort purges the part directory, so they are file
and write-lock work, not "cheap point queries". `routes.c` already declared the whole `/api/uploads*` prefix RR_WORKER.
httpd.c now sends EVERY /api/uploads verb through `upl_thread`; the only `uploadapi(` call left in httpd.c is the worker's
(own connection). Not done: a sqlite progress-handler query deadline for loop-side queries (the row's second half; no
loop-side heavy query remains in the five named routes, but the generic deadline does not exist).
Files: native/core/httpd.c, native/tests/unit/test_routes.c (`test_uploads_off_loop`: all 15 method x path combos are RR_WORKER; httpd.c has no `uploadapi(g_db`, exactly one `uploadapi(db,`).

### API-11 FIXED (limiter design)
Findings (a) TOTP replay was fixed earlier. This pass: (b) per-IP key collapses behind a proxy, (c) table pressure / no global ceiling.
- `ratelimit_client_key()`: X-Forwarded-For is believed only when the TCP peer is listed in `$JO_TRUSTED_PROXIES`
  (comma-separated exact addresses); then the right-most entry that is not itself a trusted proxy is the client. Untrusted
  peer: header ignored whole (no bypass by sending one). Garbage in the header from a trusted proxy: falls back to the peer.
  IPv6 collapses to its /64; v4-mapped to v4.
- `ratelimit_breakglass_allow()`: 5 / 60 s per client key (unchanged), then a global ceiling `$JO_BREAKGLASS_GLOBAL_PER_MIN`
  (default 60, 0 disables) over all keys. Client bucket is charged first so a refused client does not spend the shared ceiling.
- Lockout properties: both are fixed windows, so a refusal ends one window after an attacker stops; one client's flood cannot
  lock another client (different key). The global ceiling is the one shared lever: an attacker sustaining it can still
  deny the emergency path while they keep sending, which is the price of a pre-auth brute-force cap; it is sized (60/min)
  well above legitimate use. I did not add refund-on-success or per-user exemption (over-build).
- httpd.c break-glass handler passes the XFF header. The audit row still records the peer IP (unchanged).
Files: native/core/ratelimit.{c,h}, native/core/httpd.c, native/tests/unit/test_ratelimit_breakglass.c (new).

### API-17 FIXED
camera_proxy_fetch's lookup now has `AND record_type IN ('camera','camera-discovery')` (the pair camera_refframe.c uses;
camera_store.c writes 'camera'). Any other intel item, by full uid or bare id, is 404 "camera not found" and its URL is never
dialled. 415 on image/svg stays. Note: a cache hit is checked before the DB read, but entries are only written after a
camera-row fetch succeeded, so it cannot serve a non-camera item.
Files: native/core/cameraproxy.c, native/tests/unit/test_cameraproxy.c (news item with an image URL -> 404 by full and bare uid; a `camera-discovery` row still reaches the upstream refusal 502).

### ARC-01 FIXED (disclosure), live path not testable here
- lib/ws.c: reassembly moved into `ws_asm_feed()` (public, testable without a socket); it now also counts bytes dropped
  (`ws_last_oversize_dropped_bytes()`); `ws_message_ceiling()` exposes the limit.
- lib/jocore.h: `jo_ws_oversize_notice()` (+ `_n` explicit-args form) files a `collector-truncation-notice`
  (via `jo_truncation_notice_ex`) with messages_dropped, bytes_dropped, max_message_bytes, records_used, records_available null.
- bluesky_jetstream_jp.c and certstream_jp.c call it after `ws_collect()`. Nothing is filed when nothing dropped.
- The two collectors cannot run live here (libcurl 8.5.0 has no WebSocket), so only the lib level is tested.
Files: native/lib/ws.{c,h}, native/lib/jocore.h, native/collectors/sources/{bluesky_jetstream_jp,certstream_jp}.c, native/tests/unit/test_ws_oversize.c (new).

### ARC-02 FIXED
lib/seenset gained `seen_hset` (hash-only: ceiling `max_slots`, `inexact` flag, `n` count; the same FNV-1a/64 open-addressed
table, doubling at 70% load, as intel.c's hand-rolled one). core/intel.c uses it (`seen_hset_add`, `.n`, `!.inexact`,
`.max_slots = SEEN_MAX`) and the hand-rolled `seen_add/seen_put/uid_hash/seen_*` fields are deleted. Semantics kept exactly:
a new uid -> 1, dup -> 0, ceiling or ENOMEM -> 0 plus floor flag (stored>=N). The existing string `seen_set` was NOT used
(it strdup's every key: tens of MB for a 278K-record source versus 4 MB) so this is an extension, not a swap.
Files: native/lib/seenset.{c,h}, native/core/intel.c, native/tests/unit/test_seenset_hset.c (new).

### ARC-05 FIXED
lib/jocore.h gained `jo_iso_now_s` (`...SZ`), `jo_iso_now_ms0` (`....000Z`), `jo_sql_now` (`YYYY-MM-DD HH:MM:SS`) and
`jo_mono_ms()`. Replaced:
- seconds-Z copies: breach_adapter, cadence, service_vec, source_interval, sourceapi_summary (hand-rolled snprintf, same bytes), content_change (now_iso)
- .000Z copies: image_vec, entity_vec
- SQL-format copies: evidence (now_utc), content_change (now_utc_sql)
- ms copies of jo_iso_now: maintenanceapi, certstream_jp
- monotonic-ms copies (9): netjail, llm_worker, ffmpeg, story, respcache, camera_stills, intel, ratelimit, isochrone.
  hostgate.c kept (adds a test clock skew); wall-clock now_ms in collcache.c and sourceapi.c kept (not monotonic; sourceapi.c is another agent's).
test_llm_queue.c adjusted to call `jo_mono_ms()`.
Files: native/lib/jocore.h and the 20 core/collector files above, native/tests/unit/test_jocore_time.c (new: each helper is compared byte for byte with the old private copy it replaced, same wall-clock second; shapes and lengths pinned; exact-size buffers char[20]/char[32]).

## Verification (from native/, JO_DB scratch under the scratchpad)
- `make -j8`: rc=0, 0 `warning:` lines in the log.
- New/changed tests, each run via tests/unit/run.sh: test_ratelimit_breakglass, test_seenset_hset, test_ws_oversize, test_jocore_time, test_routes ("chain matchers checked: 149 / test_routes: ok"), test_cameraproxy, test_ws, test_intel_sink_write, test_intel_sink_batch (stored=400 notices=8 identical batched/unbatched), test_auth_sec, test_llm_queue (after fixing its mono_ms use): all pass.
- `make unit` full run: 114 test files; the one BUILD FAILED was test_llm_queue (used the removed private `mono_ms`), fixed and rerun alone: "test_llm_queue: all ok". A first full `make unit` was killed by the environment (Terminated, rc=143), so the second run is the evidence. Because the llm_queue fix came after that run, the final tree has not had a single uninterrupted all-green `make unit`.
- hptest: all passed. pagewalktest: ok. authtest: all passed. rsstest: all passed.
- lint-sources: OK. source-floor: 105229 >= 105229. registry-floor: 105229 >= 105229, 0 duplicates, 0 dropped, 0 never-runs.
- audit-sources: strict set 397 files, 0 finding(s).
- tools/ci_concurrency_gate.sh ./bin/japanosint 1.0 (PORT=4291): "gate: ok".

## Not changed / caveats
- No sqlite progress-handler deadline (API-07 second half).
- ws collectors' notice path was not exercised against a live socket.
- Break-glass audit rows still record the peer IP, not the forwarded client.
