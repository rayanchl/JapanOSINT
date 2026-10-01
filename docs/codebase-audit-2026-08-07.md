# OSINTsaas — full codebase audit

_2026-08-07 · branch `feat/osint-batch12-c-port` · 12-agent sweep + orchestrator verification_
_Scope: `native/` (C engine), `ios/`, `client/`, build, tests, docs. Read-only — no source was modified._

This audit re-verifies every prior report in the repo and sweeps the tree fresh.
It is deliberately adversarial about earlier claims: **prior reports were treated as
hypotheses, not facts**, including their own `VERIFIED` / `MEASURED` tags. Where a
precedent claim failed re-checking, that is recorded as a finding in its own right.

---

## 0. The five things that matter most

1. **The branch does not compile from a clean clone.** Four `core/` files and one
   collector header are untracked but `#include`d by tracked code.
2. **The search index silently drains.** `intel.c` deletes an item's FTS row, then
   fails to re-insert it against a v1 index — and the flag the header *recommends*
   is what puts you in that state.
3. **The catalog lies in both directions.** 67 sources are advertised with no
   implementation; ~2,175 implemented sources have no metadata row.
4. **`alertsapi.c` has no authorization check at all** — a `viewer` can create a
   webhook rule that exfiltrates the tenant's entire intel stream.
5. **36% of the collector codebase exists only in this working tree**, with no CI
   and no backup — alongside 35 MB of untracked prior-audit work.

_Counterweight, because the list above is one-sided:_ the **July 31 – Aug 1 audit work
largely landed** — `FIXPLAN.md` is P1 4/4, P4 13/13, P5 4/4, and 12 of `SUMMARY.md`'s 16
cross-cutting defects are fixed and complete (§8b). What did **not** land is the
2026-08-02 `CODEBASE_SWEEP.md` tier, plus nine fixes that reached some call sites and not
their siblings.

---

## 1. Ground truth (measurements corrected)

Several numbers in circulation across prior reports are wrong. These were re-derived
and cross-checked by at least two independent methods.

| Quantity | Circulating value | **Actual** | Why the old number was wrong |
|---|---|---|---|
| Registered `source_def`s | 1,182 / 1,186 / 1,998 | **≈2,562** | `grep REGISTER_SOURCE` counts *text lines*; `RSSX()` expands to a full `source_def` **plus** its own `REGISTER_SOURCE` (`arxiv_feeds.c:5-13` yields 41 sources from 1 line) |
| — direct registrations | — | 1,103 | 1,182 text lines − 79 macro bodies (`REGISTER_SOURCE(SYM)`×47, `(sym)`×32) |
| — macro-expanded | — | 1,459 | RSSX 1,117 + RSS 92 + ~250 across 77 other macros |
| Collector `.c` files | 584 / 630 / 649 | **1,016** | 649 tracked + **438 untracked** |
| `source_registry.gen.c` rows | 412 / 415 / 420 | **387** | its own header comment says 415 |
| gen.c rows with no implementation | 92 | **67** | 66 = working-tree deletions, +1 pseudo-source `osint-search` |
| Registry metadata coverage | "~33%" | **387/2,562 ≈ 15%** | followed from the source-count error |
| `git status` entries | — | **781** = 464 `??` + 251 `M` + 66 `D` | — |

Two consequences follow immediately. `MAX_SOURCES 4096` (`registry.c:9`) has far less
headroom than the "1,186" reading implies. And the drift between the two registries is
roughly twice as bad as previously reported.

**Duplicate source IDs: none.** Verified across all ~2,551–2,562 registrations with a
macro-expanding extractor, cross-checked against `collectors/existing_ids.txt` (0 misses).
One case-only near-miss worth a deliberate decision: `RELIEFWEB` (`aid_world.c:341`) vs
`reliefweb` (`intel_gov_disaster.c:366`) — `registry_get` uses exact match, so no collision.

---

## 2. Ship blockers

### 2.1 A clean clone does not compile

| Untracked (on disk only) | `#include`d by (tracked) |
|---|---|
| `native/core/fts_schema.{c,h}` | `core/db.c:7`, `core/intel.c` |
| `native/core/hostgate.{c,h}` | `core/httpclient.c`, `core/scheduler.c` |
| `native/collectors/sources/denki_yoho.h` | 10 tracked `*_power.c` collectors |

The Makefile globs `core/*.c` and `collectors/sources/*.c`, so the local build silently
absorbs them. The last recorded build (`native/tests/audit/build_check.log`, 2026-08-01,
WSL) links `obj/core/fts_schema.o` and `obj/core/hostgate.o` — confirming they are
build-required, not optional.

> **Correction to a subagent finding:** `core/source_registry_dyn.c` was reported as a
> fifth untracked file. It is **tracked**, and defines `src_meta_at`/`src_meta_count`
> at `:89`/`:99` for callers in `intelapi.c:655,664` and `miscapi.c:187,189,240,242`.
> The `/api/layers` fix links fine on a clean checkout.

Also untracked: **438 collector `.c` files** (36% of the collector codebase), the only
unit test (`tests/unit/test_dispatch_pool.c` + `run.sh` — so `make unit` and
`make tsan-test` both fail on a fresh clone), and the entire `native/tests/audit/` tree.

### 2.2 The FTS index drains on every re-ingest — *verified directly*

`intel.c:91-95` deletes the item's existing FTS row. `intel.c:116-118` then prepares an
INSERT naming **nine** columns (`uid,title,body,summary,keywords,link,author,tags,props`).
Against a v1 five-column `intel_items_fts`, `prepare_v2` fails, `inserted` stays 0, and
`:133-135` logs and **returns — after the DELETE has already committed**.

Scheduled collectors re-emit continuously, so the index bleeds rows over time with no
error surfaced to any API.

The trap is documented backwards. `fts_schema.h:48-50` says:

> *"Set `JO_FTS_REBUILD=0` to skip it for one boot (search keeps working against the v1
> index, just without the new columns)"*

That flag is precisely the v1-index state in which every write is a net delete.

### 2.3 The catalog is wrong in both directions

```
source_registry.gen.c rows ............  387
  └─ with no implementation ...........   67   → /api/layers returns 200 + empty
registered source_defs ................ 2,562
  └─ with no metadata row ............. ~2,175   → cannot create a map layer
```

All 66 collectors deleted in the working tree still carry rows in `source_registry.gen.c`,
plus live references at `core/statusapi.c:156`, `core/intelapi.c:415` (`boj-stats`,
`jcg-navarea`, `nict-atlas`) and `ios/JapanOsintApp/Onboarding/OnboardingFlow.swift:600`
(`twitch-jp-streams` — a dead tile in the intro carousel).

Independently: geometry-emitting scheduled sources with **no layer from either registry**
have grown from a reported 21 to **~240–304**. Their pins are collected daily and can
never render.

---

## 3. Security

**Verified clean** (stated explicitly so effort isn't wasted here): no SQL injection
anywhere — `dbexplorerapi.c` allowlists tables at `:13-19`, validates `orderBy` against
real `PRAGMA table_info` at `:66-69`, and `fts.c:136-143` correctly doubles quotes. No
command injection (`ffmpeg.c` uses argv-only `execvp` with a `-protocol_whitelist` that
excludes `file`). No static file server exists. TLS verification is never disabled
(zero `VERIFYPEER`/`VERIFYHOST` hits). `.env` was never committed across all 261 commits.
CSPRNG throughout; tenant secrets are AES-256-GCM with per-tenant HKDF. The RS256→HS256
"fallback" is **not** exploitable: `verify_jwks` hard-rejects non-RS256/ES256 at
`auth.c:212`, and `verify_hs256` keys only from `SUPABASE_JWT_SECRET` (`auth.c:51-54`),
never from a JWKS public key.

### Confirmed issues

| Sev | Finding | Location |
|---|---|---|
| **High** | **`alertsapi.c` has no authorization check at all** — `grep -cE "role\|role_rank\|403"` → **0**. The function isn't even passed `tenant_ctx`, so it cannot authorize in principle. A `viewer` can create a webhook rule (validated only as `http(s)://` — no allowlist, no private-IP block) that exfiltrates every matching item, delete colleagues' rules, and use `POST /api/alerts/:id/test` as an SSRF primitive that returns the response body. `savedsearchapi.c:1056` gates the equivalent action — so this is an oversight, not a design choice. | `alertsapi.c:239`, `httpd.c:1240` |
| **High** | **`entityapi.c` has no tenant predicate.** The only `tenant` reference is `:109`, which *selects* the column without filtering on it; `httpd.c:1857-1928` never calls `tenant_resolve`. Siblings `casesapi.c:342`, `aoiapi.c:639`, `exportapi.c:515` all filter correctly. | `entityapi.c` |
| **High** | **Breach corpus dumpable with plain auth.** `intelapi.c:211` reroutes `?source=<breach slug>` to `breach_adapter_list()`, which emits cleartext breached identifiers. `/api/breach/search` gates identical data behind `opgate_check` with the comment *"Breach data is sensitive, so gate it like /api/admin."* A gate on one of four doors is not a gate. | `intelapi.c:211` |
| **High** | `exp` checked only *if present and numeric* — a token with no `exp`, or a string `exp`, **never expires**. No revocation list. `aud` skipped when it's an array (RFC 7519 permits it); `iss` never validated. | `auth.c:262-273` |
| **High** | `GET /api/tenant-keys/:name` returns the **full plaintext** key; `can_manage()` is true for any member under `all_members` policy, so a `viewer` exfiltrates every stored key. `keysapi.c` has **zero** `audit_write` calls. | `keysapi.c:365-390` |
| **High** | **No rate limiting anywhere in the server.** Pre-auth break-glass TOTP login is unthrottled (`BREAK_GLASS_ENABLED=0` by default, and the minted token is inert against this server — but it would be discovered broken *during* an emergency). An unknown `kid` forces a fresh outbound JWKS fetch per unauthenticated request. | `httpd.c:463-480`, `auth.c:214-228` |
| **High** | **No response-size ceiling** and `CURLOPT_ACCEPT_ENCODING ""` means libcurl inflates *before* `on_data`. A 1 MB gzip → tens of GB → OOM. No protocol pin, no private-IP block, `FOLLOWLOCATION=1` with `CURLOPT_REDIR_PROTOCOLS` never set. | `httpclient.c:61-71,134` |
| **High** | **HTTP method not enforced on the two heaviest write paths.** 14 explicit 405 guards exist across ~75 routes; these two aren't among them. A GET fans out 16 scrapers; a GET on `/api/intel/sources/:id/run` wrote 474 rows. Any link-prefetching client mutates state. | `httpd.c:726`, `httpd.c:2025` |
| **High** | **Export filters silently dropped → full-dataset egress.** `qs[2048]` truncates while `format` is read from the *untruncated* query: pad 2,100 bytes and you get a correctly-named CSV containing everything. Enterprise row cap is `-1`. | `httpd.c:1269-1275` |
| **Med** | LLM-proposed collector URL overrides have **no SSRF validation**, inherit the collector's auth headers, and are **irreversible** — the in-memory list is append-only with no removal function. | `url_override.c` |
| **Med** | `ref_key` derived from the **raw** URL, so `api_key`/`token` query params are stored in cleartext and reachable via the API. The adjacent evidence path redacts correctly via `redact_url`. | `content_change.c:1217` |
| **Med** | Tenant invites claimed on an **unverified** JWT `email` claim — the only theoretical cross-tenant crossing found. | `tenantapi.c:106-144` |

**Tenancy is latent, not live.** All three `intel_sink_make()` call sites write
`tenant_id = 'legacy'`, so there is no tenant-private intel to leak *yet*. It becomes a
full corpus dump the moment per-tenant collection ships. `pipeline.c:174` writes every
user's search into the shared bucket, which a tenant predicate would **not** fix.

> Your working-tree `.env` holds live `SUPABASE_JWT_SECRET` and `SECRETS_MASTER_KEY`.
> The latter roots the HKDF for every tenant's encrypted keys — compromising it decrypts
> all of them at once.

---

## 4. Correctness

### 4.1 Memory safety — critical

1. **`certstream_jp.c:126-133`** — heap `o += snprintf` accumulator where `blen`
   (`:125-126`) budgets the domain list but **not** `iss`. The prefix costs
   `18 + strlen(iss)` where `iss` is upstream-controlled `leaf_cert.issuer.O`. A normal
   DigiCert DN is 78 bytes, so `bo > blen` and `body + bo` is past the allocation with
   `blen - bo` wrapped to ~`SIZE_MAX`. **Triggers on ordinary CT traffic**, at a 60 s
   interval. `malloc` at `:127` also unchecked; `it.title = cn` at `:160` may be NULL.
2. **`station_clusterer.c:848`** — `seg_cell_push` stores raw interior pointers
   (`&ix->pool[ix->pn++]`) into a `realloc`-grown array. At segment #1024 the pool moves
   and all 1,024 cached `seg_t *` dangle; `:989` dereferences them and `:995` `strcmp`s
   a `char *` read from freed memory. Fires on any rail network >1024 coordinate pairs —
   i.e. every real run.
3. **`pipeline.c:264-287`** — `pthread_join(th[i])` walks `[0, started)`, but `started`
   is a *count*, not the successful indices. One `EAGAIN` at `i=0` joins a calloc-zeroed
   `pthread_t` (UB) and leaves a live worker running `dispatch_one` against `free`'d
   memory and `run_tasks`'s dead stack frame.
4. **`fts.c:9-18`** — `utf8_next` reads past the allocation and steps over the NUL.
   Not reachable via `?q=` (that destination is a zeroed 256-byte stack buffer), but live
   on every exactly-sized heap string: `intel.c`'s per-item `fts_write`,
   `fts_schema.c:218-226`'s corpus rebuild, and `alert_eval.c:414`
   `fts_segment(x->valuestring)` — making `POST /api/alerts` with `{"q":"…\xF0"}` an
   authenticated remote trigger. Duplicated at `translate.c:89-98` and
   `station_clusterer.c:76-91`. `entitystore.c:39-56` already does this correctly.
5. **`snprintf` accumulators: 12 dangerous of 86** (previously reported as 8 of 28 — the
   earlier sweep missed *casted* forms, the very form its own exemplar used). Confirmed
   unguarded: `p2pquake_jma.c:80,87,92,94` (`char t[320]`, unbounded `place` from feed
   JSON), `unified_flights.c:46-49` (guard present only on the fourth of four),
   `wolfx_eew.c:80-81`, `reg_no_brreg.c:26,33` (loop `break`s *after* `j += n`, then the
   post-loop write runs unconditionally on a 512-byte stack array).
6. **`breach_index.c:199-218`** — eviction clears `f` and `path` but not `lru`, so after
   any `fopen` failure the next call re-picks the same slot and executes `fclose(NULL)`.
   Deterministic once the disk fills mid-ingest.
7. **`ffmpeg.c:363-364`** — `pipe()` without `O_CLOEXEC`; `media.c:833`'s `popen()` on
   another worker inherits the write end, so EOF never arrives and a healthy ffmpeg is
   SIGKILLed. Lose that race once in `probe_tools`' `pthread_once` and `g_ff_ok` stays 0
   **for process lifetime** — every camera reports `rtsp_requires_ffmpeg` forever.
8. **`httpd.c:342-350`** — `mg_wakeup` is `alloca(len+8)` on a detached thread's stack,
   sent over a **UDP** socketpair with `send()`'s return ignored. macOS caps datagrams at
   9216 bytes: a large `/api/search/suggest` body is dropped and the request hangs forever.
9. **`linegeom.c:130-135`** — only the outer array length is checked; `[[1,2],[3]]` makes
   `cJSON_GetArrayItem(pt,1)->valuedouble` a NULL deref on stored OSM geometry.

Notably clean: `docmeta.c`, `content_change.c`, `simhash.c`, `station_footprints.c`,
`uploadapi.c`'s path-traversal defence, the EXIF/JPEG/PNG parsers, `auth.c`'s b64 sizing.
Collector memory handling is genuinely good — **zero** files declare more `http_response`
than they free; one real leak found in 1,016 files (`geoeo_usdm_drought.c:102`, `pj`).

### 4.2 Database & pipelines

**Is `intel.c` `emit()` the single chokepoint? Partly.** It is the only
`INSERT INTO intel_items` (`intel.c:35`), but two live writers change
`intel_items.properties` without touching `intel_items_fts.props`:
`station_clusterer.c:1573` and `camera_geocode_pod.c:205`. A geocoded camera address is
visible in the detail pane and permanently unfindable by search.
`DELETE FROM intel_items` **does not exist anywhere** — so the FTS-on-delete question is
moot and it's a retention bug instead: nothing ever deletes an `intel_items` row.

**Is DB concurrency safe? No.** The design is right for the scheduler pool, dispatch pool,
alert threads and evidence GC — all `db_attach` their own connection. It is violated at
`httpd.c:500`, `:741`, `:2064`, which hand the event loop's shared `g_db` to detached
threads running `emit()`'s `BEGIN`/`COMMIT`, concurrently with handlers that
`BEGIN IMMEDIATE` on the same handle (`uploadapi.c:676`, `aoiapi.c:754`, `casesapi.c:805`).
A concurrent upload's `ROLLBACK` discards the collector's rows; its `COMMIT` publishes
half-written state. `scheduler.c:123-129` documents exactly why this is illegal and does
it correctly. Also `pipeline.c:294`, `breach_monitor.c:401`, `translate.c:616` — whose
comment gives the wrong justification (`SQLITE_THREADSAFE=1` serializes API calls, not
transactions).

`entitystore.c:119` is the **unfixed twin** of the bug `intel.c:109-113` documents as
fixed: `last_insert_rowid()` after a discarded step points entity X at entity Y's rowid,
and the next write evicts Y from search.

### 4.3 Collectors

- **44 `run()`s report an honest empty as an error.** Worst: `jma_earthquake.c:189`
  `return n > 0 ? 0 : -1` at a **60 s** interval — a quiet hour with no new quakes returns
  `-1`, `scheduler.c` feeds it to `anomaly_detect`, and the map's flagship source is
  quarantined **for working correctly**. Also `grid_usage_realtime.c:134`,
  `vessel_finder.c:105`, `sans_isc.c:75`, `ipa_alerts.c:15`, `sakura_front.c:182`. Two
  return the row count as a status code (`crypto_onchain.c:132`, `patent_search.c:86`) —
  and `license_plate.c:68-70` carries a comment explaining this exact fix, unpropagated.
  Inversely, 253 files use `return n >= 0 ? 0 : -1` with a never-decremented counter:
  right outcome, unreachable branch. Two templates in circulation.
- **613 collectors on `news.google.com` at one request every 5.3 s** —
  `gnews_world_topics_1.c` (218) + `_2.c` (185) + `gnews_world_top.c` (142) at 3600 s,
  plus `gnews_osint_monitors.c` (68) at 1800 s = 681 req/h. `reddit_world_geo.c:3-26` is a
  written post-mortem of this exact failure: at 1800 s, **135 of 142** reddit sources
  returned `rc=-1` with zero rows because `rss_collect` maps 429 → −1 → quarantine; the fix
  was 7200 s (~51 s spacing). Google News sits ~10× more aggressive using the identical
  `RSSX` body. Throttling is host-correlated, so **~24% of the fleet quarantines on one
  trigger**.
- **313 gnews definitions contradict their own URLs** — the es-419 template was
  copy-pasted, so `gnews-us-world`, `gnews-gb-business`, `gnews-fr-world` display
  "Google News World — Latin America (es-419)" / "ラテンアメリカ" while fetching
  `hl=en-US&gl=US`, `hl=en-GB&gl=GB`, `hl=fr&gl=FR`. Only ~55 URLs are genuinely es-419.
  This is the only analyst-facing text in the dashboard.
- **5 sites concatenate unescaped data into JSON/query syntax** — `certstream_jp.c:154`
  (CT issuer DN → `tags_json`, corruption **persisted**), `ioc_lookup.c:75`,
  `patent_search.c:39`, `fofa_jp.c:152`, `credential_leak.c:76`.
  `reg_ua_prozorro.c:117-123` has the team's correct escaping loop.
- **Invented geometry, 11 sources.** `denki_yoho.h:52` names the field
  "control-centre / HQ coordinates" and emits it as the row's Point with **no
  `geo_precision`** — 10 power collectors pinned to Tokyo/Nagoya/Osaka/Fukuoka for
  region-wide demand, at 300 s. `airquality_world.c:366` defaults to Tokyo for any
  non-coordinate pivot, with `has_geo = 1` at `:431`.
- `academic_search.c:147-148` writes `d[10] = 0` after copying only `c < 10` bytes,
  publishing up to 10 bytes of uninitialised stack as the record's `date`.

---

## 5. Fabricated data — the `SOURCE_REALITY_REPORT` claim does not survive

That report audited 61 collectors and generalized to *"No collector fabricates
measurement data … Pure fabricated/stub data: none found."* The tree registers ~2,562
sources — a 2.4% sample. A later report in this same repo (`native/tests/audit/SUMMARY.md`,
2026-07-31) already contradicts it with a section titled **"Fabricated geolocation — found
and removed"**.

**Credit where due, and it is substantial:** `real_data` / `is_real` / `is_live` now have
**zero occurrences repo-wide**. 13 of 16 spot-checked PARTIAL collectors are genuinely
fixed (`vehicle_lookup.c:201-203`, `flight_tracker.c:114`, `company_lookup.c:97-99`). All
4 NEEDS_KEY collectors now emit *nothing* rather than a misleading note — stricter than
described. Portal-probe stubs went ~73 → 7. Both Python generators are clean: they emit
only `rss_collect(...)`, and `lib/rss_atom.c:269-397` is a real fetch that returns −1 on
any non-2xx with no fallback payload. **1,117 of 1,117 generated collectors are real
fetches; 0 are URL-stub emitters.** A flaw there would have replicated 1,117 times.

**Surviving counter-examples:**

| Collector | What it fabricates |
|---|---|
| `wifi_networks_mls.c:11-33,44-66` | 20 invented sequential BSSIDs (`00:1A:79:00:00:01`…) at landmark coords, **zero fetch in the file**, emitted as `"source": "mozilla_mls"`, every 86400 s. Key-gating makes it worse: fake rows appear only when an operator expects real MLS data |
| `wanted_persons.c:321-336` | Named wanted individuals on an index-derived ring around police HQ; `caseCount` = max of three unaligned regex passes (`:317`) |
| `email_reputation.c:142-153` | Synthesizes `reputation_score = 50` when upstream returns nothing, with no "unknown" state; emits `deliverable: false` as hard fact |
| `mlit_landprice.c:162-167,219-250` | LCG ±0.01° jitter injected into emitted coordinates; `:127-131` falls back to prefecture capital |
| `geothermal_projects.c:127-137` | Hardcoded `103.5`/`73.7` MW tagged `"jogmec_geothermal_live"` |
| `npa_important_wanted.c:456-461` | Same ~4.4–7.8 km ring, `sensitive:true`, no precision flag |
| `cam_curated_jp.c:278-281` | `geo_precision:"exact"` including 4 prefecture-centroid rows |
| `satellite_tracker.c:79` | Hardcoded NYC observer; `ctx->entity` never read |
| `wifi_networks_shodan.c:53-54` | Tokyo Station fallback pin |

Three of these (`mlit_landprice`, `wifi_networks_shodan`, `cam_curated_jp`) survived the
2026-07-31 purge only because they are key-gated and the sweep never exercised them.

---

## 6. Clients

### iOS (146 files) — structurally sound, narrowly broken

The app is well-built: modern navigation, 21 `ContentUnavailableView`, disciplined 44 pt
targets, strong Japanese handling, and `SWIFT_DEFAULT_ACTOR_ISOLATION = MainActor` makes
off-main mutation structurally impossible. But:

1. **Only one Xcode target exists** (`project.pbxproj:63-86,114-116`) — one
   `PBXNativeTarget`, one synchronized root group (`path = JapanOsintApp`). All 6
   `JapanOsintWidgets/*.swift` and `JapanOsintShare/ShareViewController.swift` **never
   compile**. `Info.plist:38` declares `NSSupportsLiveActivities`, but nothing calls
   `Activity.request` and the renderer sits in the uncompiled folder — the feature is inert.
2. **`CODE_SIGN_ENTITLEMENTS` appears nowhere in the pbxproj** — all three `.entitlements`
   files are dead (the app's own file admits it at `:10-16`). `AppGroup.containerURL`
   (`Widgets/SharedSnapshot.swift:30`) returns nil forever, so widgets, the share queue,
   and `BackgroundRefresh` are silent no-ops burning background budget.
3. **No `/ws` server exists** — `httpd.c:1974-1975` says so verbatim. `WebSocketClient.swift:56`
   sets `isConnected = true` before the handshake; `reconnectAttempts` resets only on a
   `"connected"` frame that never arrives → **permanent 30 s reconnect loop for the app's
   lifetime**, and `CameraDiscoveryView.swift:59` falsely shows `OfflineStateView`.
4. **Alert-rule edits always 400** — `AlertEditor.swift:279` sends the masked `"••••"` back;
   `alertsapi.c:214-218` requires ≥16 chars and the mask is 12 bytes. Renaming a webhook
   rule is impossible.
5. Reachable crashes: `CameraFeedView.swift:130` force-unwraps a URL built from unvalidated
   scraped `youtube_id`; `API.swift:618` force-unwraps from the user-editable base URL;
   `TimeSliderView.swift:301` builds `lowerBound ... upperBound` from two independently-
   published optionals and traps when the newest data is older than 7 days — routine
   between ingests, on the map's most prominent control.
6. **`try!` is now present** — `CameraFeedResolver.swift:52,60,70`, introduced by the camera
   rework. The prior report's "zero `as!`/`try!`" no longer holds.
7. Camera grid still instantiates unbounded `WKWebView`s and `AVPlayer`s with **zero
   `dismantle*` hooks** (`CameraVideoPlayer.swift:27-42`) — and this survived the two
   commits that rewrote those exact files. `:41` swaps in a new `AVPlayer` without pausing
   the old.
8. `Theme.swift:135-142` uses `.system(size:)` without `relativeTo:` across ~50 call sites,
   ignoring Dynamic Type entirely; ~25 more hardcode below the 11 pt floor. Given your HIG
   preference, this is the widest conformance gap. `mapBarSurface` is violated 9 times on
   the bars themselves (8 `.thinMaterial` + `TimeSliderView.swift:457` `.thickMaterial`,
   which the prior report missed).

**New bugs in the uncommitted diff:** `IntelTab.swift:143` reads the uncached computed
property `childSources` (`:220-229`) *inside* `ForEach` — each access runs `filteredSources`
twice plus a full Set and dictionary build. At ~2,000 sources that's ~3 array passes + 2 Set
builds per materialised row; the pre-diff code was a single `ForEach(filteredSources)`.
`SourceDashboardTab.swift:253` documents this exact trap and avoids it there.
`expandedParents` (`:27`) is never pruned.

### React client (14,678 LOC) — effectively abandoned

`launch.sh` contains zero occurrences of "client"; the C server has no static handler and
404s every non-`/api` path (`httpd.c:2141`); one commit in 30 days touched it, and its own
message says the edits *"ride along."* iOS is the live client.

Its real-time layer targets the `/ws` server that was never ported — five hooks reconnect
forever against a 404. 8 endpoints it calls no longer exist (root cause: `httpd.c:188`
`seg()` rejects multi-segment tails), including `/api/data/cameras/snapshot`, so **every
camera thumbnail is a broken image**. 10 of the map's core layers have no collector — and
they are the *first ten* `LAYER_DEFINITIONS` entries (`earthquake`, `weather`, `transport`,
`air-quality`, `radiation`, `population`, `landprice`, `river`, `crime`, `gdelt`): the
layers the README advertises as the product are exactly the dead ones.

Genuinely clean: **zero** `innerHTML`/`dangerouslySetInnerHTML` in the whole tree, no
secrets, 34 `fetch(` vs 33 `res.ok` checks.

---

## 7. Build, tests, CI, hygiene

**There is no CI anywhere** — no `.github/`, `.gitlab-ci.yml`, `.circleci/`, `Jenkinsfile`,
no active hooks, despite a live GitHub remote. For 438k LOC of C the entire safety net is:
one untracked unit test covering `pipeline.c`, a destructive contract suite, a bench script
with a dead path, and a `--selftest` flag.

- **`launch.sh:211-214` — a failed build reports success.** `make`'s status is discarded
  twice (it's first in a pipeline, and the whole thing is `if ! …; then :; fi`, which
  defeats `pipefail`). The only remaining check is `[ -x "$BIN" ]` — **a stale binary is
  still executable**, so a broken compile boots old code and prints "build green".
  `native/tests/audit/agent_build.sh:14-18` documents this exact bug as already fixed there;
  it was never carried back.
- **`tests/contract/run.sh` destroys its own committed fixtures.** `:75` captures from
  `$ROOT/server/src/index.js`, deleted 2026-05-17. The safe `cmp` fallback triggers only
  when node is absent — node v24.18.0 *is* installed, so a bare `./run.sh` takes the capture
  branch, Node fails, and `:110` runs `curl … > "$HERE/$f.node.json" || true`: **`>`
  truncates before curl fails and `|| true` hides it.** One invocation zeroes ~8.7 MB of
  tracked baselines. Re-capture is impossible on any host — `server/` no longer exists in
  git either.
- **`tests/bench/run.sh:12` still hardcodes `ROOT=/Users/rayan/JapanOSINT`.** The prior
  report attributed this to `contract/run.sh` — that one *was* fixed; `bench/run.sh` was
  missed.
- **`native/obj- ` (trailing space) explained.** Not a Makefile bug: `git log -p --all --
  native/Makefile` shows exactly one `OBJ` line, `+OBJ := obj`. It came from
  `tests/audit/agent_build.sh:20-26`, where `SLOT="${1:?…}"` rejects unset/empty but **not
  whitespace**, and `OBJ="obj-$SLOT"` then `cp -a obj "$OBJ"`. It holds 1,480 files. Win32
  strips trailing spaces from path components, so it is unreachable by name and warns on
  every `git status`; deleting it needs a `\\?\` prefix.
- **`make clean` orphans 616 MB.** `:116-117` removes only `$(OBJ)`, `obj-tsan`, `bin`;
  `obj-*` on disk totals **758 MB**, all unignored (`.gitignore` covers only `native/obj/`).
- **`CFLAGS` is destroyed by any command-line override** (`:38-47`) — `make CFLAGS=-O0`
  silently drops `-DJO_REPO_ROOT`, `-MMD -MP` and `-Ithird_party`, disabling the very `.d`
  mechanism the comment at `:40-45` exists to guarantee. Needs `override CFLAGS +=`.
- **12 files hardcode `/Users/rayan/JapanOSINT`** as the `JO_REPO_ROOT` fallback (`db.c:16`,
  `prompts.c:16`, `breach_meta.c`, `main.c`, +8), and `grammar_load` **caches the empty
  string on failure** (`prompts.c:637-644`), so a wrong root is permanent and silent for the
  process lifetime — the LLM just runs ungrammared. *(The grammars themselves are fine: all
  7 `.gbnf` exist and `REPO_ROOT := $(abspath ..)` derives correctly from the checkout. The
  earlier "portability gap" conclusion was wrong; the missing-warning half stands.)*
- **No `asan` target** despite four of the critical findings being heap UAF/over-read.
  `bin/japanosint-asan` exists but was built by hand once. TSan *is* wired
  (`Makefile:102-114`) with a correct separate object tree, but `obj-tsan/` has 677 `.o` vs
  `obj/`'s 1,108 — stale by ~a source generation — and there's no recorded race count.
- **63.4 MB of tracked llama binaries**, ~92% off-platform on any given machine, of which
  ~22 MB is pure triplication (soname symlinks committed as full copies: `libllama.so`,
  `.so.0`, `.so.0.0.10068` are three identical blobs).
- **Five platform assumptions in one repo**: Makefile → macOS/Homebrew, `launch.sh` → macOS
  (sets `DYLD_LIBRARY_PATH` while `native/llama/` holds 38 `.so` vs 7 `.dylib`),
  `agent_build.sh`/`build_asan.sh` → Linux, `tests/audit/` → WSL paths, vendored libs →
  Linux. **No single machine runs all of it.** This Windows checkout has no `make`, `cc`,
  `gcc`, `clang`, `pkg-config` or `mecab-config` at all.

### Documentation is contradicted in six places

Six different wrong source counts are enshrined in tracked files: **313**
(`P5_REMAINING.md:4`), **286** (`P6_SWEEP_STATUS.md:6`), **318**
(`OSINT_ENGINE_STATUS.md:4`), **476** (`launch.sh:63` `EXPECT_SOURCES`), **~551**
(`registry.c:7`), **150+** (`README.md:3`). Actual: ~2,562.

`README.md` still documents `server/ Express + SQLite + node-cron` and `npm run dev` for a
Node backend deleted 2026-05-17, and **never mentions the iOS app or the C engine**.
`docs/collectors.md` documents `server/src/collectors/` with 206 camelCase JS names
(`jmaEarthquake`) against kebab-case C ids (`jma-earthquake`). `docs/pipeline.md` is not
about the pipelines at all — it's the breach design plan, and it self-contradicts on the
shard key.

### Repo junk

Confirmed: a 0-byte file at the repo root named `| tail -5` — `git status` shows
`?? " \357\201\274 tail -5"`, and octal `\357\201\274` is **U+F07C, a Private Use Area
glyph**, so the pipe was mangled through a font round-trip before hitting the filesystem.
Also `native/out1/` (empty, unignored), `native/tests/contract/__pycache__/` (no
`__pycache__` rule despite 61 `.py` files), tracked `docs/password-management.md.txt`
(double extension), and `native/.run/server.log` at **32 MB**, never rotated.

### The meta-finding

`native/tests/audit/` — **843 files, 35 MB, 38 markdown reports, 205 scripts** — is
entirely untracked. That includes `CODEBASE_SWEEP.md`, `SUMMARY.md`, `FIXPLAN.md`,
`DECISIONS.md`, `REMOVED_SOURCES.md` and `report_01`–`report_10`. Every prior audit, every
measurement harness, and the only unit test vanish on `git clean`. Combined with 438
untracked collectors and no CI, **the single highest-risk fact in the repo is that most of
the recent work exists only in this working tree.**

---

## 8. Unification

Measured fresh: 1,016 collectors / 160,411 LOC (107,910 non-comment) · core 44,899 · lib
3,597. **The top-120 normalized line-forms cover 30.9% of the collector tree.**

Collector-level items from the July report *did* land — `plane_adsb.c` deleted,
`unified_flights.c:74` repointed to `flight-adsb`, breach stubs gone, 88 probe stubs gone
(`probe.h` down to 7 includers). The helper-level work did not, and every count grew.

| # | Opportunity | Files | LOC | Risk |
|---|---|---|---|---|
| 1 | `lib/jocore.h` — 16 canonical micro-helpers (594 copies) | 420 | ~5,190 | mechanical |
| 2 | `lib/osintemit.h` — one OSINT emit envelope (147 fns) | 112 | ~5,100 | moderate |
| 3 | `intel_item` envelope + `rc_fetch_array` fetch/parse guard | 591 / 687 | ~3,600 | moderate |
| 4 | `gj_point_feature()` in `lib/geojson.h` | 240 | ~1,300 | mechanical |
| 5 | `core/` micro-helpers (`ctext`×21, `uuid4`×15, `err`×18, `b64url*`×19) | ~45 | ~1,470 | mechanical |
| 6 | Retire `source_registry.gen.c` (dual registry) | 1 + 387 | ~420 + drift | risky |
| 7 | `core/dbutil.h` + one `INTEL_COLS` (437 prepare/step/finalize sites) | 45 | ~1,050 | moderate |
| 8 | One HTTP wrapper — promote `jo_get`, convert 117 raw users | 117 | ~600 | mechanical |
| 9 | `GET /api/vocab` + delete hand-copied Swift/JS enums | ~9 | ~150 | moderate |
| 10 | One `DEFINE_RUN_SOURCE` macro, delete 55 variants | 80 | ~430 | mechanical |
| 11 | `httpd.c` tenant×18 / opgate×8 / body-copy×36 preambles | 1 | ~200 | mechanical |
| 12–17 | csv adoption, `rss_collect_ex`, table-driven `reg_*`, one sink decorator, central `record_type`, delete `_person_links.inc` | ~80 | ~3,770 | mixed |

**≈12,000 LOC mechanically removable; ≈22,500 including moderate-risk (~14% of collector+core).**

Three observations that reframe the work:

- **The debt is in blocks, not files.** A full pairwise normalized-line Jaccard across all
  1,016 collectors found **1 pair ≥0.72 and 7 clusters ≥0.55** — "delete N cloned files" is
  a small win. The LOC lives in intra-file blocks: `intel_item` envelope 8,454 lines across
  591 files; fetch-parse guard 3,030/687; Point-Feature 1,781/240; raw HTTP 733/117.
- **`lib/` is ignored, not dead.** No lib module lacks callers, but: feedlib has 426 users
  against **117 raw `http_request` + 131 via a third wrapper `jo_get`**; geojson 121 users
  against **240 files building Features inline**; csv 20 against 26 fetchers + 41 private
  splitters. `jo_get` is strictly better than `feed_get_text` (Shift_JIS transcode), so
  `feed_get_text`'s 111 users are silently missing that fix. **Cheapest first PR: 66 files
  include `_jp_osint.inc` *and* still define their own duplicate helpers.**
- **Registration macro sprawl: 55 distinct names, 80 files, 284 expansions** — `RSSX`×18,
  `DEFR`×4, `RSS`×4, `DEF`×3, plus **50 singletons**. That's 55 places the `source_def`
  field list can drift from `source.h`.

**Client contracts: 22 defined in 2+ languages, 9 disagree, 3 are live bugs.** No
OpenAPI/codegen exists; 189 Swift `Codable` structs are hand-mirrors. Live drift:
`AlertPredicate.record_types` (`Models.swift:765`) is a user-settable filter the C engine
never reads (`grep record_types native/` → 0); `CaseRefType.all` is missing `attachment` —
the exact drift `annotationsapi.h:59-66` was written to prevent; and a 21-message-type
WebSocket protocol across two clients with no server implementation. Also 445 distinct
`record_type` literals with no central list, and 28 category values in gen.c vs 19 in
`.category=` literals (disjoint by 12+3) while `/api/status` emits the union of 31.

**Deliberately left alone** (documented rationale exists): `lib/camfeature.h` vs the 14
`make_feature()` copies (`camfeature.h:16-21`), overpass's omitted cache/politeness queue,
the GeoJSON-over-RSS rewrite of geo-hazard feeds (`intel_gov_disaster.c:34-38`), the two
LLM calling conventions, and the `source_def` metadata blocks themselves.

---

## 8b. The July 31 – August 1 audit work largely *did* land

This deserves saying plainly, because §2–§4 read as unrelieved bad news and that would be
unfair. The `native/tests/audit/` ledger (`SUMMARY.md`, `FIXPLAN.md`, `DECISIONS.md`,
`REMOVED_SOURCES.md`) verifies as **28 FIXED · 11 CONFIRMED · 7 STALE · 2 WRONG · 9
half-applied**.

`FIXPLAN.md` is close to complete: **P1 4/4 · P2 6 done + 1 partial · P4 13/13 · P5 4/4**,
with P7 the main gap (1 done, 1 partial, 5 not done). Of `SUMMARY.md`'s 16 cross-cutting
defects, **12 are genuinely fixed and complete** — `rss_atom` ×6, `httpclient` ×2, the
geojson null-guard, the camera_store link, `_jp_osint.inc` ×4-in-1, overpass ×2. All 17
quarantine-on-success collectors are fixed. So the earlier sweeps were acted on; it is
specifically the *2026-08-02* `CODEBASE_SWEEP.md` tier that wasn't.

Two `SUMMARY.md` claims are **wrong in the safe direction**: `hudson-rock-jp` is recorded
as "left, flagged" but was actually removed (`hudson_rock_jp.c:54-62`), and `DECISIONS.md`
"Kept deliberately" lists 6 broken scrapers of which 5 were deleted the next day.

### The nine half-applied fixes

This is the highest-value output of the ledger — each is a fix that landed at some call
sites and not others.

| # | Fixed | Missed |
|---|---|---|
| 1 | `add_tokyo_geom()` gutted in 13 files | 4 registered siblings still pin: `nicter_stats.c:125-129` (NICT HQ — named in the *same* report_03 batch as osv-dev/ghsa, which *were* fixed), `leakix_jp.c:55-56`, `mlit_transaction.c:77-78`, `wifi_networks_shodan.c:53-54` |
| 2 | Phase-6 latent-quarantine fix reached `jshis_seismic.c:66` | Its 6 other *named* files untouched: `data_go_jp_ckan.c:126`, `egov_laws.c:133`, `hatena_bookmark_extended.c:253`, `japan_reit.c:191`, `jma_earthquake.c:189`, `sakura_front.c:182` (44 fleet-wide) |
| 3 | Quarantine-on-success fixed in 17 collectors | Never fixed at the root — `core/scheduler.c:70` untouched |
| 4 | RFC-822 → ISO switched in the **write** path | The backfill `FIXPLAN §2.3` mandated does not exist; the DB sits in exactly the "worse before better" state the plan warned about |
| 5 | `DECISIONS §7.6` applied to `npa_special_fraud.c:223` | It kept the pin but `publisher-hq` has **0 hits repo-wide**, so it is unfilterable; `hudson_rock_jp.c` did the *opposite* of the decision |
| 6 | `geo_precision` made honest across all 11 camera aggregators | `cam_curated_jp.c:280` stamps `"exact"` on all 194 rows, incl. **5** on prefecture centroids from the investigation's own stacking table (`:231,:232` Kanagawa, `:233,:234` Osaka, `:239` Tokyo) |
| 7 | `camera_geocode_pod.c` written to un-stack cameras | It filters `geo_precision IN ('prefecture','city')` at `:234` — so those 5 rows are **invisible to the very pod built to fix them**; and both it and `cam_curated_jp.c` are **untracked** |
| 8 | geojson defect #8 patched in `hudson_rock_jp.c:66-70` | Patched per-collector instead of in `lib/geojson.c` |
| 9 | `record_type="service-portal"` removed from 5 collectors | 1 survives fleet-wide |

Two candidates were chased and **cleared** (not bugs): `station_footprints.c:110` is guarded
by `compute_bbox` returning non-zero at `:49-53`, and all four `overpass_*` returning raw
`n` are wrapped as `n >= 0 ? 0 : -1` by every caller.

### Camera verdict

The two recent commits fixed the real headline symptom — **visibility and linkability**
(`camera_store.c:26-40,439` keys on the uid keyspace; `:237-238` sets `link`) — and
`geo_precision` is now honest across all 11 aggregators. They did **not** fix stacking:
11 files still carry `PREFECTURE_CENTROIDS`, `cam-webcamtaxi` is still broken, and the fix
that would work is untracked and blinded to 5 rows by #6 above. `camera_upsert` still never
sets `summary`/`tags`. The "179 cameras" figure is stale — it is 194 today.

### Cross-report contradictions

Fifteen were found and adjudicated against current code. The three that matter:

- **The "no fabricated data" conclusion** is scoped to a directory that no longer exists and
  is contradicted today by `wifi_networks_mls.c:12-32` — a compiled-in table of 20 invented
  BSSIDs with coordinates, still registered. (§5.)
- **`report_09`'s "all predecessor work holds"** is contradicted by four other slices *and*
  undermined at the tooling level: `report_01.md:132-135` records the build script reporting
  OK on a **failed** build against a 40-minute-stale binary. Every "verified live" claim from
  that window carries an unquantified error bar. This is the same defect as
  `launch.sh:211-214` (§7) — which means the bug did not just risk shipping bad code, it
  already corrupted an audit.
- **`report_10`'s "no `rc > 0` bug in these 83 files"** is a scan-method artefact.

Also: `REMOVED_SOURCES.md` is internally inconsistent — its −187 header never absorbs the
17 further gnews retirements listed in its own later sections. Its non-gnews set **is**
exactly the 66 working-tree deletions (53 probes + 8 key-gated + 5 scrapers).
`bom-au-warnings` is the only removal completed end-to-end with zero registry rows left.

### Dangling-reference union (final)

All 66 rows in `source_registry.gen.c`; **3 ids each** — not 1 — in `statusapi.c:156-157`
and `intelapi.c:415-416` (`boj-stats`, `jcg-navarea`, `nict-atlas`), in verbatim-duplicated
arrays; `OnboardingFlow.swift:600` (`twitch-jp-streams`); and **8 frozen contract fixtures**
under `native/tests/contract/`. Nothing in `docs/`.

### Plan docs

`PHASE1.md` **contradicted** · `feature-roadmap-plan.md` **completed** (both its premises —
"no iOS UI exists" and "alertsapi is CRUD-only" — are now false) · `breach-ingest-revamp-plan.md`
**completed** · `ios-attachments-plan.md` **abandoned** (backend is real,
`ios/JapanOsintApp/Uploads/` was never created) · `ios-extensions-plan.md` **abandoned and
self-contradicting** — 7 Swift files, ~54% of its output, never compile (§6.1), and it
claims `Intents/AddToCaseIntent.swift` is committed when it isn't ·
`osint-sources-1000.md` accurate except "~727 registered" → ~2,562 · `breach-sources.md`
**partial** — it cites `docs/breach-check-pipeline.md` eight times and **that file has never
existed**; the seed TSV is **1,171** rows, not the 1,018 documented, and
`breach-corpus.json` is stale at 1,018.

---

## 9. The pattern worth naming

**Every fix already exists somewhere in this tree.** This recurred independently across
four agents:

| Fixed here | Still broken here |
|---|---|
| `intel.c:109-113` rowid-after-failed-step | `entitystore.c:119` |
| `scheduler.c:123-129` per-thread `db_attach` | `httpd.c:500,741,2064` |
| `scheduler.c` checks the `emit` return | `osint_dispatch.c` ignores it |
| `lib/zipread.c` `inflate_raw` bomb ceiling | `zip_first_entry` (`:174`) |
| `unified_flights.c` guards its 4th accumulator | `:46`, `:48`, `:49` |
| `savedsearchapi.c:1056` role gate | `alertsapi.c` (no gate at all) |
| `entitystore.c:39-56` bounded UTF-8 decode | `fts.c:9-18`, `translate.c:89-98` |
| `agent_build.sh:14-18` build-status check | `launch.sh:211-214` |
| `tests/contract/run.sh` portable ROOT | `tests/bench/run.sh:12` |
| `reg_ua_prozorro.c:117-123` JSON escaping | 5 concatenation sites |
| `license_plate.c:68-70` honest-empty comment | 44 `? 0 : -1` collectors |
| `SourceDashboardTab.swift:253` hoists the computed property | `IntelTab.swift:143` |
| `add_tokyo_geom()` gutted in 13 collectors | `nicter_stats.c:125`, `leakix_jp.c:55`, `mlit_transaction.c:77`, `wifi_networks_shodan.c:53` |
| Latent-quarantine fixed in `jshis_seismic.c:66` | Its 6 co-named files, and `core/scheduler.c:70` at the root |
| geojson defect patched in `hudson_rock_jp.c:66-70` | `lib/geojson.c` itself |

The dominant failure mode is **silence**: discarded `sqlite3_step` returns, unchecked
`COMMIT`, `UPDATE OR IGNORE`, `LIMIT`-before-filter, `|| true` after a truncating redirect,
`[ -x "$BIN" ]` as a build check. That is what makes these dangerous rather than merely
wrong — and it is also why nine of these are mechanically checkable by a linter that
doesn't exist.

---

## 10. Recommended order

**Before anything else — stop the bleeding (minutes):**
1. `git add` `core/fts_schema.{c,h}`, `core/hostgate.{c,h}`,
   `collectors/sources/denki_yoho.h`, the 438 collectors, `tests/unit/`, and
   `native/tests/audit/*.md`. Nothing else on this list survives losing them.
2. Fix the FTS delete-then-fail (§2.2) or force the rebuild; the index is degrading now.
3. `.gitignore`: add `native/obj-*`, `native/out1/`, `__pycache__/`, `*.pyc`; drop stale
   `server/data/`.

**Today (hours, high certainty):**
4. `CREATE INDEX idx_intel_items_pub` + `cache_size`/`mmap_size`/`ANALYZE` — the prior
   sweep measured 35.1 s → 0.002 s on `/api/intel/items`, and it remains unfixed.
5. `alertsapi.c` — pass `tenant_ctx`, gate six routes (pattern at `savedsearchapi.c:1056`).
6. Move the breach gate inside `breach_adapter_list()` / `_reveal_by_uid()`.
7. Bound `utf8_next` (port `entitystore.c:39-56`); clamp the 12 `snprintf` accumulators.
8. `certstream_jp.c:126-133` and `station_clusterer.c:848` — both fire on normal traffic.
9. Method guards on `httpd.c:726` and `:2025`; reject over-long query strings with 414.
10. `launch.sh:211-214` build-status check; `tests/contract/run.sh` capture branch.

**This week:**
10b. Close the **nine half-applied fixes** in §8b — each is a decision already made and
    partially executed, so they are the cheapest correctness wins available and carry no
    design debate. Start with #3 (`core/scheduler.c:70`, the root cause behind 17
    already-patched collectors) and #7 (`camera_geocode_pod.c:234` is blind to the 5 rows
    it exists to fix).
11. Own DB connection per off-loop thread at `httpd.c:500,741,2064`.
12. `jma_earthquake.c:189` + the 43 other quarantine-on-empty collectors.
13. Back Google News off to ~7200 s (the number `reddit_world_geo.c` already derived).
14. iOS: add the widget/share targets and `CODE_SIGN_ENTITLEMENTS`, or delete the dead
    files and the `NSSupportsLiveActivities` claim. Fix the four crashes and the
    `IntelTab.swift:143` regression before it ships.
15. An `asan` Makefile target — four of the critical findings are heap errors.

**Structural:**
16. `make lint-sources` + CI. Nine of the §9 rows are mechanically checkable and none is
    checked. Start with: duplicate ids, orphan curated rows, geometry-without-layer,
    the `? 0 : -1` shape, unguarded `snprintf` accumulators, `last_insert_rowid` after an
    unchecked step.
17. `lib/jocore.h` + `lib/osintemit.h` (§8 items 1–2, ~10,300 LOC).
18. Retire `source_registry.gen.c`; make metadata inline so a new source can create a layer
    without hand-editing a generated file.
19. `GET /api/vocab` to kill the hand-copied client enums.
20. Decide the fate of `client/` — repair it or delete it, but stop shipping a README that
    advertises it as the product.

---

## Appendix — method

Twelve subagents ran read-only in parallel: precedent-report verification ×4
(`backend-structure-and-pipeline-report.md`, `SOURCE_REALITY_REPORT.md`, `CODEBASE_SWEEP.md`
backend + clients), plus native-core bugs, collector sweep, security, pipeline/DB, iOS,
client/build/hygiene, unification, and an audit-tree ledger. The orchestrator independently
verified the untracked-header build break, the registry drift counts, the source-count
reconciliation, the FTS delete-then-fail path, and the recorded build log.

Claims are marked by how they were established. Two subagent findings were **overturned**
by orchestrator checks and are recorded as such: `source_registry_dyn.c` is tracked (§2.1),
and the grammars portability gap does not exist (§7). Timing figures carried from
`CODEBASE_SWEEP.md` (35.1 s sort, 725 µs/row `emit`, 38.2 s aggregates) are **not
re-measured here** — no `.db` was opened — and are cited as prior measurements whose
underlying code paths were re-confirmed unchanged.
