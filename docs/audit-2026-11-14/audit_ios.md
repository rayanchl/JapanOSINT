# iOS audit vs C server (static, read-only), 2026-11-14

Scope: /home/user/JapanOSINT/ios (JapanOsintApp; JapanOsintShare + JapanOsintWidgets are not built) against native/core/httpd.c, statusapi.c, sourceapi.c, intelapi.c, dataapi.c, thresholds.c. Web client: client/src. Paths below are relative to ios/JapanOsintApp unless noted. Nothing was built or run; every claim is from reading code.

## 0. Headline

iOS treats the registry as a ~1,700-row list (the comments say so: SourceDashboardTab.swift:261-263, IntelTab.swift:155-160). At 61,527 rows its source surfaces download the whole fleet on every open, hold it in memory and group and filter it on the main actor. None of the server's paged and searched source endpoints are used. A user cannot search the registry on the server, cannot see a source's notices, runs or provenance, cannot edit per-source settings, cadence or interval, and cannot run an entity pivot. "Run now" on a pivot reports success while doing nothing.

## 1. Endpoint inventory (iOS call -> server route -> model)

All routes below exist on the server with the method iOS uses, except `POST /api/admin/restart` (see IOS-09). That includes every route in API+Roadmap.swift, checked by grep in httpd.c.

| iOS call (file:line) | Method + path | Decodes into | Server check |
|---|---|---|---|
| API.swift:196 | GET /api/me | MeResponse (all optional except user.id, tenant.id/slug/name) | OK, tolerant |
| API.swift:200 | GET /api/health | raw Data | OK |
| API.swift:204 | GET /api/layers | [LayerDef] (Models.swift:41) | OK. The server sends null for category, data_type, modality and records_geocoded, and all are optional in Swift |
| API.swift:207 | GET /api/sources | [DBRowAny] | OK, SELECT * of sources, the full 61k rows (intelapi.c:861) |
| API.swift:212-214 | PUT /api/sources/:id/schedule | DBRowAny | OK, operator-gated (httpd.c:2834) |
| API.swift:217 | GET /api/status (no limit, no summary) | StatusEnvelope / StatusRow (Models.swift:371,412) | Fields match statusapi.c:305-420. `sched`, `view`, `vectorIndexes`, `intelTotal*` and `probe*` are sent and ignored. All Swift fields are optional, so there is no decode risk. It returns every row (httpd.c:1527-1531) |
| API.swift:223,233 | POST /api/status/:id/probe, /consent | StatusRow | OK. The consent body key is `consent` (httpd.c:2912) and iOS sends it |
| API.swift:246 | GET /api/data/cameras/discovery-feed | manual JSON | OK |
| API.swift:262-285 | GET /api/data/:layerId | FeatureCollection (+_meta) | OK, but `_meta.truncated`, `records_available` and `next_offset` are ignored (IOS-08) |
| API.swift:288 | GET /api/intel/sources (full) | IntelSourcesEnvelope | `name` and `item_count` are non-optional in Swift; the server always sends both (intelapi.c:1190-1197). Extra `geocoded`, `ungeocoded`, `awaiting_geo` and `is_intel` are ignored |
| API.swift:310-340 | GET /api/intel/items | IntelItemsEnvelope | OK |
| API.swift:347,358 | GET /api/intel/semantic, /items/:uid/similar | SemanticEnvelope | OK |
| API.swift:365,371,377 | GET /api/intel/items/:uid[/entities, /reveal] | IntelItemEnvelope etc. | Routes exist. The uid is NOT percent-encoded (IOS-10) |
| API.swift:387 | POST /api/intel/sources/:id/run | IntelRunResult | OK. The server replies `ran:true, ingested:0` for a pivot run with no entity (IOS-05) |
| API.swift:397-400 | GET /api/geocode, /reverse | | OK |
| API.swift:408-424 | GET /api/db/tables[/:t], /api/db/scheduler | | OK, operator |
| API.swift:429,432 | GET /api/follow/recent, POST /api/data/cameras/trigger | | OK |
| API.swift:440-486 | /api/keys, /api/tenant-keys, /tenant-keys/policy | ApiKeyMeta, TenantKeysEnvelope, KeyPolicyResponse | OK |
| API.swift:493 | POST /api/admin/restart | none | Server returns 501 not_implemented (httpd.c:3186-3188) (IOS-09) |
| API.swift:499-530 | /api/admin/maintenance, /source/:id, repairs approve and reject, sources unquarantine, anomalies requeue | MaintenanceDigest, SourcePipeline, OkResult | Routes exist (httpd.c:3191-3263, 3379). The paged `lists/:name` and `source/:id/{fetch_log,anomalies,repairs}` routes are unused |
| API.swift:544-591 | /api/admin/breach/* | | OK |
| API.swift:602-618 | GET/PUT /api/admin/thresholds[/:key] | SimilarityThreshold (Models.swift:814) | Field-for-field match with thresholds.c:238-253. `env_value`, `updated_by` and `updated_at` are optional, so it decodes |
| API.swift:665-724 | /api/alerts[/:id][/mute\|unmute\|test\|events] | AlertRule etc. | Routes exist |
| API.swift:732-752 | /api/members[/invite][/role] | | OK |
| API.swift:758-809 | /api/search/analyze, /suggest, /results/:id, SSE /search/stream/:id, /api/entities/* | SearchSnapshot etc. (SearchModels.swift) | All fields are optional or tolerantly decoded. `stream_key` and the `stats` block are ignored |
| API+Roadmap.swift:28-640 | cases, annotations, timeline, alert-events, aoi, watchlists, breach-monitors, saved-searches, search-history, permalink, evidence, media, camera-stills, visual-search, export, isochrone, capture-* | many | Routes exist. Not field-diffed (see "Not verified" below) |

Not verified (time-boxed): per-field diffs for the roadmap models (cases, timeline, aoi, watchlists, evidence and others). I read only the source, threshold, status, intel, layer, search and me models.

Server routes with NO iOS caller (the coverage matrix proper):

| Server route | Used by iOS? | Used by web? |
|---|---|---|
| GET /api/sources/search (server-side filter, facets, paging) | NO | yes (SourcesListPage.jsx:69) |
| GET /api/sources/:id/detail | NO | yes (SourceDetailPage.jsx:221) |
| GET /api/sources/:id/summary | NO | yes (:222) |
| GET/PUT /api/sources/:id/settings (body ceiling, `interval_sec` facade) | NO | yes (:46,63) |
| GET/PUT/DELETE /api/sources/:id/interval | NO | indirectly, via the settings key |
| GET/PUT /api/sources/cadence (system-wide policy) | NO | yes (CadenceCard.jsx:23) |
| GET /api/sources/semantic, /categories | NO | NO (server only) |
| GET /api/sources/stats | NO | yes (useDataSources.js:20) |
| GET /api/collectors, /api/collectors/:key | NO. Collectors are rebuilt client-side from the full /api/status | yes (CollectorsPage.jsx:151, CollectorDetailPage.jsx:23) |
| GET /api/status?summary=1 / ?limit=&offset= | NO (always the full dump) | yes (summary=1) |
| GET /api/status/:id | NO | no |
| GET /api/sources/:id/logs (run history) | NO | yes (link) |
| GET /api/search/runs (workspace run history) | NO | yes (WorkspaceRuns.jsx:52) |
| GET /api/layers/:id/geojson?limit&offset (bounded, states totals) | NO (uses /api/data/:id) | n/a |
| GET /api/admin/maintenance/lists/:name, source/:id/{fetch_log,anomalies,repairs} (paged) | NO | yes (AdminPage.jsx:46-48) |
| GET /api/entities/:type/:id/merges; graph `rel_types`, `exclude_hubs`, `max_nodes` | NO | not checked |
| POST /api/intel/sources/:id/run | yes, in 7 places: SourceDashboardTab.swift:567,609, IntelTab.swift:347,357, IntelSourceItemsView.swift:354, IntelSourceRow.swift:92, SourcePipelineView.swift:217, BreachCorpusView.swift | yes |
| Running a pivot with an entity directly | NO endpoint exists for any client. Pivots run only through /api/search/analyze (LLM dispatch) | same |

What an iOS user CAN do for sources: browse the collector -> source tree (dashboard) with client-side search over id and name only; open a thin detail card; use "Run now" (non-internal rows); probe and set consent for key-gated rows; see health and trust; browse a source's items (IntelSourceItemsView, paged by cursor); flip schedule mode (operator); see the maintenance and repair pipeline (operator); edit similarity thresholds (operator).

Cannot: search the 61k on the server (name, host, description, collector, health, grade filters); see notices or failure reasons (`collector-fetch-failure` and shape notices) as notices; see provenance, data shape or run history; edit per-source settings, interval or cadence; see the cadence policy and its runs/day estimate; list the workspace's search runs; run a pivot with a chosen entity.

## 2. Findings

Severity is for the stated owner goal ("every collector / source usable in iOS").

| ID | Sev | Title | Evidence | Suggested fix | Conf |
|---|---|---|---|---|---|
| IOS-01 | critical | The dashboard downloads and holds the whole registry (~61.5k rows, estimated 75-80 MB) on every open and refresh, with a 120 s ceiling | SourceDashboardTab.swift:621 `api.status()`. API.swift:217 (no limit). The server's own note says default-unbounded is kept only for clients like this (httpd.c:1527-1531). The server measured 20.5 MB / ~8-25 s for 16,367 rows (httpd.c:1527-1531), so ~77 MB at 61.5k. API.swift:36-38 `timeoutIntervalForResource = 120`. The server's 10 s TTL cache (httpd.c:1565) does not help a cold build | Switch the dashboard to `/api/status?summary=1` for the cards, `/api/collectors` for the grouping and `/api/sources/search` (limit 50, `offset`, `has_more`) for rows. Fetch `/api/status/:id` or `/detail` on open. Adopt the web client's shape (SourcesListPage.jsx:65-72) | high |
| IOS-02 | critical | Main-thread O(n) and O(n^2) work over the full row set on every state change | SourceDashboardTab.swift:234-238,295 (one `Circle()` per source in the status chart, 61k views). :445 `groupedAsCollectors()` over all rows on each keystroke of the search box. :427,:436 `apis.filter{...}.count` per facet item. SourceScheduleSettingsView.swift:30-33,61,75: `grouped` recomputed each body, a non-lazy `Form` with one segmented Picker per source, and `rows.first(where:)` inside each row's Binding getter, so O(n^2) (~3.8e9 comparisons for 61k) | Server paging (IOS-01) makes this go away. Meanwhile cap the status chart to counts, not dots; use `List` or `LazyVStack`; key rows by dictionary | high |
| IOS-03 | critical | Intel tab loads all of /api/intel/sources (~25-40 MB, one entry for every registered source), then rewrites 61k SwiftData rows on the MainActor, and again after every single Run tap | IntelTab.swift:314-318 `intelSources()` then `intelCache.cacheSources(env.data)`. Persistence/IntelCache.swift:11,36-42: `@MainActor`, `try? context.delete(model:)`, then `context.insert` per source; the comment says "dozens of rows". IntelSourceRow.swift:92-93 and IntelTab.swift:291 `onRunComplete -> reload()` re-run the whole thing. TimelineModel.swift:285 does the same download. Server builds the list with a linear scan per source (intelapi.c:1186-1187) | Page the catalogue, e.g. `?limit=&offset=` (fleet_params supports them, httpd.c:1557-1563) or `/api/sources/search?has_items=1`. Do not cache the catalogue wholesale; cache only has_items rows; refresh a single row after a run | high |
| IOS-04 | critical | "Run all sources" in the Intel toolbar fires POST /run for every source in the registry | IntelTab.swift:329-366 loops `sources` (all ~61k, cap 4 in flight) with `try? await api.intelRunSource(id)`: no confirmation, no operator check, errors swallowed. The server's own rate limit for non-operators is 10 runs per 60 s (httpd.c:3011-3014). For an operator it is unbounded and it also hits pivots (no-ops) and pods | Remove it, or scope it to a selected collector with a count confirmation (the dashboard already refuses >12, SourceDashboardTab.swift:598). Surface failures | high |
| IOS-05 | high | "Run now" on a pivot (kind `pivot` / `scheduled-pivot`) reports success, but nothing ran | SourceDashboardTab.swift:891 enables Run for everything except `kind == "internal"`. Server httpd.c:3036-3078 calls `scheduler_run_source(...,NULL)` and replies `ran:true, ingested:0`. hpengine logs "on-demand pivot and no entity was given" to stderr only (lib/hpengine.c:4575-4590). The UI prints "ran: 0 new rows". The same applies to Run all in a pivot collector (SourceDashboardTab.swift:602-614) | In the UI, disable Run for `kind == pivot` and offer an entity field that posts to a pivot-run endpoint. The server has no such endpoint (it is also a web gap): add `POST /api/intel/sources/:id/run {entity}`. Server side, return an explicit `needs_entity` instead of `ran:true` | high |
| IOS-06 | high | Long runs always report "failed" in the app though the server finishes them: the comment says no client timeout, the code sets 120 s | API.swift:34-38 (`timeoutIntervalForResource = 120`), :112-117 (`timeout: nil` becomes 120), contradicting the doc at :55-58 and :378-386. Big sources take far longer: ~72 min for the QLD CKAN walk, and a 945-page read at 1.5 s per page (CLAUDE.md). The server then holds the single-flight slot (httpd.c:3016), so the next tap gets 409 `run_in_flight` | Make the run an async job: client fires and then polls `/api/sources/:id/detail` (or `/logs`), or use a longer per-call resource timeout for `/run` | high |
| IOS-07 | high | Operator access hardcoded to one email address | Auth/PlatformAdmin.swift:12 `accountEmail?.lowercased() == "<contact address>"` gates Admin, thresholds, breach corpus, DB explorer, repair worker, schedule (RootView.swift:191, ConsoleHub.swift:94, AdminPanel.swift). The server decides by PLATFORM_OPERATOR_EMAILS (httpd.c opgate_check). Web probes the server instead (client/src/auth/AuthContext.jsx:56-66, GET /api/media/capabilities) | Same probe on iOS, or add `is_operator` to /api/me. Also removes a personal email from the binary | high |
| IOS-08 | high | Map layers silently truncated, with no in-band disclosure (house rule 2) | The server caps `/api/data/<layer>` at 10,000 (LAYER_FC_DEFAULT_LIMIT, dataapi.c:287) and states `_meta.truncated / records_available / next_offset` (dataapi.c:463-473). iOS decodes `_meta` (Models.swift:293-304) but only reads `bySource` (MapTab.swift:1004,1035). Then `visibleFeatures` stride-decimates to `maxFeaturesPerLayer` (default 500, PersistedModels.swift:140) and 200 for shapes with no "showing N of M" (MapTab.swift:819-872) | Show "N of M" per layer from `_meta` plus the decimation count; page with `/api/layers/:id/geojson?limit&offset` | high (no UI string found; I did not read every map overlay) |
| IOS-09 | high | Server restart button reports success on a 501 | API.swift:492-495 `POST /api/admin/restart`. Native server answers 501 not_implemented (httpd.c:3186-3188). AdminPanel.swift:208-218 swallows any error as "expected connection reset", polls /health (which is already up) and plays the success haptic. The comment at API.swift:489 still says `node --watch` | Remove the control, or surface the 501 | high |
| IOS-10 | medium | Unencoded uid in three item routes | API.swift:366,372,378 interpolate `uid` raw, while `similarItems` (API.swift:359-362) encodes it with the comment "uids carry `|` and `:`, which a URL string cannot hold raw". `makeURL` uses `URLComponents(string:)`, which returns nil for a raw `|`, so these calls would throw `.badURL`, blanking item detail, entities and reveal. API+Roadmap.swift uses `esc()` consistently | Percent-encode in all three | medium (depends on the OS's URLComponents leniency) |
| IOS-11 | medium | Engine notice kinds are rendered as findings | Models.swift:720-725 `isCollectorNotice` knows only `collector-truncation-notice` and `collector-status-notice`. The server's rule is `collector-*-notice` plus `collector-fetch-failure` (core/intel.c:128-132, lib/hpengine.c:6032, core/scheduler.c:745), and `collector-shape-notice` is written by hpengine and osintemit.h:158. The fetch-failure notice, the shape notices and `collector-fetch-error` (_jp_osint.inc:141) fall through to `findingRow` in IntelItemRow.swift:15-22 and in every list. The "truncated" pill also labels every non-status notice (IntelItemRow.swift:33) | Prefix-match `collector-` + `-notice`, plus fetch-failure and fetch-error, and render the reason fields | high |
| IOS-12 | medium | Dashboard filters omit `pending`, `kind` and `engine`, and there is no pivot or pod view | SourceDashboardTab.swift:30 `StatusChoice` = all/online/degraded/offline/gated. `unknown` is lumped into pending (:243). `kind` only appears as a text row in SourceDetail (:808). Pods (collector `_...`) are not distinguished from feeds | Facets from `/api/sources/search` `meta.facets` (`kinds`, `healths`, `engines`, `groups`), as the web does | high |
| IOS-13 | medium | Source detail is a thin card made from the list row | SourceDashboardTab.swift:756-855. No notices, run history, data shape, provenance, interval edit or settings, though `/api/sources/:id/detail`, `/summary`, `/settings` and `/interval` exist. A dead source's `collector-fetch-failure` reason (HTTP status, TLS cause, remedy) is not shown. Only breach rows get "View records" (:882); other rows must be found again in the Intel tab | Add Detail, Notices, Runs, Settings and Cadence sections (mirror web SourceDetailPage.jsx) | high |
| IOS-14 | medium | Request settings fight the offline UX | API.swift:39 `waitsForConnectivity = true`: while offline, requests wait up to the 120 s resource timeout, not the 25-30 s request timeout, so `OfflineStateView` appears two minutes late | `waitsForConnectivity = false`, or a per-request short override | medium |
| IOS-15 | medium | Stale-response race in per-source item search | IntelSourceItemsView.swift:102-107 spawns a new `Task { sleep 250ms; reload() }` on every keystroke and never cancels the previous one, so N reloads fire and an older response can overwrite a newer one. Same shape in CrossSourceSearchView (not checked) | `.task(id: searchText)` with a debounce (auto-cancels) | high |
| IOS-16 | medium | ApiKeys and Breach screens pull the full status dump just to decorate a view | ApiKeysView.swift:300,311 (`try? await statusTask`, swallows failure, for non-operators too). BreachCorpusView.swift:480-483 needs the breach rows only | Use `/api/sources/search?collector=…` and `?summary=1` | high |
| IOS-17 | low | Widgets and Share extension are dead code | `project.pbxproj` has a single app target (5 `PBXNativeTarget` mentions, one product type). ios/EXTENSION_TARGETS_TODO.md confirms: 7 files never compile and `AppGroup.isConfigured` is false (Widgets/SharedSnapshot.swift:59). Even when added, `latestQuake: nil` and `activeWarnings: []` are hardcoded (Widgets/WidgetSnapshotBuilder.swift:51), so QuakeWidget would always read empty. The Info.plist declares BGTask ids and background modes that do nothing without the App Group. `drainShareQueue` handles several queued items by calling `showSearch` in a loop, so only the last wins (JapanOsintApp.swift:177), and images stay queued forever | Add the targets per the TODO, or drop the code and the plist entries. Fix the queue drain | high |
| IOS-18 | low | SSE search stream sends the bearer without the credential-origin guard | API.swift:815-820 omits `mayCarryCredentials` (API.swift:82-95) that `send()` applies at :133. ATS blocks public cleartext, so exposure is limited to LAN or `.local` http | Reuse the guard | high |
| IOS-19 | low | Release default backend is the developer's Mac hostname, plain http | BuildConfig.swift:24-29 `http://Rayans-MacBook-Pro.local:4000`; Persistence/PersistedModels.swift:134 seeds it. ATS only allows local networking (Info.plist: `NSAllowsLocalNetworking`), plus a cleartext exception for `insecam.org` and subdomains | Empty default with a required field for release builds; drop the insecam exception if not needed | high |
| IOS-20 | low | Stale or drift-prone copy and lists | Threshold screen text "None has been measured on the production model" (Console/SimilarityThresholdsView.swift:49; same in client ThresholdsTab.jsx:95) contradicts CLAUDE.md (measured 2026-10-23). Onboarding says "100+ collectors / layers" (OnboardingFlow.swift:375,646) against ~640 collectors and ~61.5k sources. Hardcoded showcase layer ids (OnboardingFlow.swift:614-628), layer colour and name override tables (LayerRegistry.swift:52-170) and a category order (Layers/LayersTab.swift:207) can drift from /api/layers. The layer ids already needed manual repair in the comments | Read counts from /api/status?summary=1; keep overrides as cosmetic only | high |
| IOS-21 | low | Force unwrap and unbounded accumulations | Auth/OAuth.swift:162 `scenes.first!`. Only 2 force unwraps found in the whole tree (Onboarding:941 is a plain loop with a guard-free `.first!` on a non-empty array). IntelSourceItemsView `items.append(contentsOf:)` without bound (:365-372). Single-page fixed limits: AlertEventsView.swift:144 (200), CasePickerSheet.swift:295 (200 cases), SourcePipelineView.swift:155 (`prefix(12)` runs with no "12 of N") | Guard the optional; label bounds | medium |

Checked and fine: tokens are in the Keychain (Auth/KeychainStore.swift:12-77, AfterFirstUnlock); the Supabase anon key is a publishable key; 401 refresh is coalesced and only replays GET (API.swift:67-85, AuthTokenBox.swift); the dashboard fetch is async and decodes off the main actor; ATS has no `NSAllowsArbitraryLoads`; `realtimeWebSocketEnabled = false` avoids a reconnect storm on the missing /ws.

## 3. Web vs iOS parity (source and collector features)

Web only: server-paged source search with facets and URL-state filters (SourcesListPage.jsx); collector index and detail pages with their own paging and sort (CollectorsPage/CollectorDetailPage); source detail, summary, settings, interval and notices; cadence card (CadenceCard.jsx); `/api/status?summary=1` tiles; `/api/sources/stats` fleet figure; workspace search-run history (WorkspaceRuns.jsx); paged maintenance lists (AdminPage.jsx:46-48).

iOS only or better: Face ID biometric gate on probe and consent (Shared/ProbeActionsView.swift:76-117); per-collector favourites with a group fold (CollectorFavorites.swift, SourceDashboardTab.swift); Spotlight, App Intents and Siri hooks.

Both: probe and consent; run now; schedule mode; thresholds editor; breach corpus console; maintenance and repair worker.

Neither: a pivot run with an entity; `/api/sources/semantic` and `/categories` (server-only); the full list is still fetched by several web pages (SourcesPanel.jsx:371, AdminPage.jsx:582, SettingsPage.jsx:298, ApiKeysPage.jsx:48), so IOS-01 is shared.

## 4. Suggested order

1. IOS-04 (remove Run all) and IOS-07 (operator probe): small and safe.
2. IOS-01/02/03/12/16: one refactor to `/api/sources/search` + `/api/collectors` + `?summary=1`.
3. IOS-05/06: honest run semantics (server `needs_entity`, async run result).
4. IOS-11, IOS-13, IOS-08, IOS-09, IOS-10.
