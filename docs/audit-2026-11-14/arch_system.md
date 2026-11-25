# JapanOSINT whole-system architecture (measured from code, native/ tree, 2026-11-14)
Scale: core/ 229 files, core+lib .c = 96,112 LOC; largest: lib/hpengine.c 6,397, core/httpd.c 5,245, core/media.c 2,343, core/camera_stills.c 2,204, lib/jsonlist.c 2,163, core/station_clusterer.c 1,750, core/scheduler.c 1,654, core/sourceapi.c 1,588, core/alert_eval.c 1,572. Repo layout: native/ (C server), client/src (React/Vite: App.jsx, api, auth, components, hooks, store), ios/ (JapanOsintApp, Share, Widgets), grammars/ (GBNF + JSON schemas), scripts/, docs/.

## 1. Process / thread model
Single process `bin/japanosint`. main.c (479 lines) loads `.env` (JO_ENV_FILE or JO_REPO_ROOT/.env, env wins; main.c ~L45), validates flags against a whitelist (L164-168), then:
| Flag | Behaviour (main.c) |
|---|---|
| (none)/--selftest | db open, integrity_check, schema object count >80, llm_healthy probe, exit (L449-479) |
| --serve | breach catalog seed load -> uploadapi_migrate -> alert_eval_init -> alert_eval_sweep_start -> scheduler_start_background -> alert_deliver_start -> evidence_gc_start -> httpd_serve(port=$PORT|4000) (L400-430); shutdown: scheduler_stop_background(3000), stop gc/sweep/deliver, db_close, then `_exit` (OpenSSL UAF avoidance) |
| --sched | scheduler_loop in foreground (L395) |
| --run <id> [entity] | one source through real sink (L318) ; --dispatch <id> <entity> (L331) |
| --list-sources / --collector-census / --dump-source-cards / --host-agents / --discover-feeds [--jobs] / --ingest <id> <file> [--type --materialize --dry-run] / --help | L347/355/221/202/206/249 |

**HTTP server**: Mongoose (third_party/mongoose.c). One event-loop thread: `while(!g_shutdown_signal) mg_mgr_poll(&mgr,100)` (httpd.c:5229). `mg_wakeup_init` for off-loop replies (L5198). Slow routes spawn a **detached pthread per request** (httpd.c L340 sa, 1496 ob, 1632 fleet, 1725 sem, 1766 apv, 2390 suggest, 3058 srcrun, 3660 iso, 5048 cam_trigger, 5087 datarun) gated by `worker_admit()` cap = WBODY_SLOTS=16 (httpd.c:760, JO_HTTP_MAX_WORKERS); reply bodies parked in 16-slot g_wbody table and delivered via wakeup. There is NO fixed worker pool. SSE: /api/search/stream/:id pre-auth (L2245) via search_stream_open. Export streaming uses XS_THREAD_CREATE (L1041). WebSocket: no server-side WS (no MG_EV_WS use found); lib/ws.c is a client used by collectors (ws_collect).

**All pthread_create sites** (file:line -> purpose)
| Site | Purpose |
|---|---|
| core/scheduler.c:1531 | collector/maint lane workers (JO_SCHED_WORKERS default 8, clamp 1-32; JO_SCHED_MAINT_WORKERS default 2, clamp 1-8) |
| core/scheduler.c:1648 | sched_thread (dispatch/timer/watchdog) |
| core/llm_worker.c:155 | one dedicated thread per LLM base_url; two lanes interactive(hi)/background(lo) |
| core/auth.c:191 | jwks_refresher |
| core/alert_deliver.c:726 | alert delivery worker |
| core/alert_eval.c:1268 | entity-watchlist sweep thread |
| core/alert_sem.c:251 | detached rule-description embed |
| core/evidence.c:1116 | evidence blob GC/reaper |
| core/pipeline.c:476 | dispatch_worker fan-out of per-service tasks in an OSINT search run |
| core/searchapi.c:201 | run_thread per OSINT search |
| core/translate.c:883 | bf_thread translate backfill |
| core/source_trust.c:251 | trust rebuild |
| core/pagination_probe.c:488 | pagination survey job |
| core/breach_jobs.c:252,367 | breach ingest / fetch jobs |
| core/breach_monitor.c:668 | rescan job |
| core/netjail.c:634,832 | ffmpeg forward-proxy conn threads / server |
| core/feeddisc.c:427 | --discover-feeds worker pool |
| core/httpd.c (10 sites above) | per-request off-loop work |
Total 33 pthread_create sites in core (lib/collectors/pod: none found).

**Scheduler** (core/scheduler.c): `lane_of()` = collector name starting `_` -> LANE_MAINT else LANE_COLLECT (L1083); per-lane cv queues, shared per-source running[] set (skip-if-running), boot ramp `sched_boot_offset` (JO_SCHED_RAMP_SEC 900), backoff/quarantine, watchdog + cancel slots, `scheduler_run_source` creates http_client + llm_client + intel_sink per run (L761-). Interval composition `sched_row_interval()` = source_interval override else cadence_apply(declared/measured) (core/cadence.c, source_interval.c). `sched_disclose_fetch_failure` files collector-fetch-failure records. sched_resume token (`source_sched_state.resume_token`). **LLM queue**: core/llm_worker.c, JO_LLM_QUEUE_MAX 64, JO_LLM_QUEUE_WAIT_FACTOR 4; embed server gets own thread because worker keyed on base_url.

## 2. core/ modules by subsystem (229 files = ~110 .c/.h pairs + defs/sql)
- **HTTP/API router & cross-cutting**: httpd.c (router + ~20 inline handlers, 5,245 LOC), cors.c, ratelimit.c, respcache.c, collcache.c, progress.c (search run progress/SSE), statusapi.c (/api/status, vectorIndexes), miscapi.c, dataapi.c, geoproxyapi.c, vocabapi.c, dbexplorerapi.c, maintenanceapi.c, sweepapi.c, tenantapi.c, keysapi.c
- **Auth/tenancy/audit**: auth.c (JWT/JWKS/break-glass), operatorgate.c (opgate_check), audit.c (hash chain), tenantapi.c, keysapi.c, credtab.c
- **DB/storage**: db.c (open, WAL, vec ext, seed, migrate), schema.sql, dbutil.h, fts.c/fts_schema.c (FTS5 + MeCab segmentation), evidence.c (content-addressed blobs), uploadapi.c, docmeta.c
- **Ingest sink/dedupe**: intel.c (emit/upsert/fts_write/alert hook), simhash.c, intelapi.c, source_registry*.c/.h, layertab.c, collectortab.c, collectors.def, layers.def, host_agents.def
- **Scheduler/cadence**: scheduler.c, cadence.c, source_interval.c, source_settings.c, source_trust.c, source_category.c, source_card.c, sourceapi.c, sourceapi_summary.c, sourceindexapi.c, url_override.c, osint_dispatch.c, pagination_probe.c
- **Outbound HTTP/politeness/TLS**: httpclient.c (curl, per-host UA, TLS cause), hostgate.c (pacing/SSRF), catrust.c (extra intermediates), netjail.c (ffmpeg sandbox), feeddisc.c
- **LLM**: llm.c, llm_worker.c, llmcache.c, llm_fault.h, prompts.c (fencing, grammars/schemas), pipeline.c (OSINT-search analysis/phase-2/synthesis), searchapi.c, suggest.c, fewshot.c, entity_enrich.c, translate.c, prompt_rank.c, relevance.c
- **Embeddings/vector**: qembed.c, embed_pod.c, service_vec.c, service_vec_pod.c, service_aliases.c, entity_vec.c, image_vec.c, semsearchapi.c, thresholds.c (+lib/vecindex.c)
- **Search/FTS/timeline**: fts*.c, searchapi.c, semsearchapi.c, timelineapi.c, nearapi.c, savedsearchapi.c
- **Entities/stories**: entitystore.c, entityapi.c, entity_enrich.c, entity_stats.c, story.c, storyapi.c
- **Alerts**: alert_eval.c, alert_deliver.c, alert_sem.c, alertsapi.c, aoiapi.c
- **Cases/reports/export/annotations**: casesapi.c, reportapi.c, exportapi.c, annotationsapi.c
- **Media/camera/ffmpeg**: media.c, ffmpeg.c, hls_check.c, camera_stills.c, camera_store.c, camera_feed.c, camera_dupes.c, camera_privacy.c, camera_refframe.c, cameraproxy.c, visualsearchapi.c, content_change.c
- **Geo/transit**: transitapi.c, isochrone.c, linegeom.c, station_clusterer.c, station_footprints.c, geoproxyapi.c
- **Breach**: breach_adapter.c, breach_index.c, breach_jobs.c, breach_meta.c, breach_monitor.c, breach_store.c
- **Admin/settings**: thresholds.c, source_settings.c, maintenanceapi.c, maint_detect.c, sweepapi.c

## 3. LLM usage (every call site)
Config: generation `LLM_BASE_URL` (llm.c:27, default :8080); suggest `LLM_SUGGEST_BASE_URL` falling back to LLM_BASE_URL (llm.c:35); embeddings `JO_EMBED_URL` (writers, lib/vecindex.c) and `JO_EMBED_QUERY_URL` (query-time, qembed; fallback to JO_EMBED_URL); `JO_EMBED_MODEL`, `LLM_MODEL`, `JO_LLM_TIMEOUT_MS`, `JO_LLM_CACHE_TTL_SEC`, `LLM_ENABLED` (gates pods), `LLM_TRIAGE_MODEL`, `LLM_REPAIR_MODEL[_HARD]`, `LLM_HEAVY/MID_CONCURRENCY` (maintenanceapi), `JO_VISION_EMBED_URL` (image sidecar, plain HTTP POST not llm_embed). Every call funnels through llm_worker (one thread per base_url, interactive lane drains first).
| Site | Function | Asks the model | Lane/env | Thread | Fenced | If LLM down |
|---|---|---|---|---|---|---|
| core/pipeline.c:791 | osint_pipeline_run | analysis: extract entities/pick services (chat_ex, JSON schema, few-shot) | LLM_BASE_URL, interactive via searchapi (llm_init, L77), JO_LLM_TIMEOUT_MS | search run_thread | query fenced USER_QUERY (prompts.c:199) | stage error code llm_unreachable/unusable_output via note_llm_stage (L89-150) + fallback corpus lookup |
| pipeline.c:913 | phase-2 rounds | follow-up service selection | same | run_thread | prompt_phase2 | stage error, run continues |
| pipeline.c:1011 / 1203 | synthesis (chat_ex, fallback llm_complete w/ grammar) | summary of results | same | run_thread | prompt_synthesis | error note; no prose |
| core/searchapi.c:233,243 | searchapi_suggest_ex | suggestions (retrieval arm first, else LLM) | llm_init_suggest (LLM_SUGGEST_BASE_URL) interactive | detached suggest_thread (httpd:2390) | UNMEASURED | returns empty suggestions[] |
| core/suggest.c:65,195 | suggest_build | embeds query/candidates via qembed | JO_EMBED_QUERY_URL | worker | n/a | NULL -> fallback LLM path |
| core/sourceapi_summary.c:312,335 | source summary | small-model summary from measured facts (schema, 400 tok, 25s) | llm_init_suggest | worker (srcrun-style) | UNMEASURED | returns status code + facts, no prose |
| core/translate.c:677 (+775,854) | translate_run_ex | JA->EN of title/body/summary into `keywords` FTS column | background lane, LLM_MODEL | pod `translate` (maint pool, `_enrich`) + bf_thread backfill | YES prompt_fence_untrusted SOURCE_FIELDS (L433) | llm failure not charged to items; retried later |
| core/entity_enrich.c:96,547 | entity_enrich_run | entity extraction (prompt_entity_extraction + GBNF) and merge judge (prompt_entity_dedup) | background, LLM_ENABLED gate in pod | pod llm-enricher | UNMEASURED (prompts.c builders) | logs "LLM unreachable", items left for next tick (L109,594) |
| collectors/pod/anomaly_triage.c:420 (+275 embed) | triage | classify collector anomaly with past-triage memory | LLM_TRIAGE_MODEL, 30s embed | pod anomaly-triage | YES ANOMALY_BUNDLE, PAST_TRIAGES (L315,401) | anomaly left untriaged |
| collectors/pod/collector_repair.c:525,740 (+462 embed) | repair | propose repair for failing collector (fast/hard model) | LLM_REPAIR_MODEL / _HARD | pod collector-repair | UNMEASURED | no repair filed |
| core/scheduler.c:777 | scheduler_run_source | provides ctx->llm to every collector (osint_dispatch uses it) | llm_init | collector worker | - | - |
| core/osint_dispatch.c:217-237 | service routing | embeds query vs service_vec for source selection | qembed | search thread | n/a | inert, returns 0 |
| collectors/sources/jp_corpus_lookup.c:317 | corpus lookup | qembed_intel_knn corpus arm | qembed | collector | n/a | -1 inert |
| core/semsearchapi.c:226 | /api/intel/semantic | embed query | llm_embed direct, JO_EMBED_QUERY_URL, 15s | detached sem_thread | n/a | 503/degrade |
| core/service_vec.c:806 | route query embed | embed | 15s | worker | n/a | keyword fallback |
| core/entityapi.c:508-511 | hybrid entity search | qembed | | worker | n/a | lexical only |
| core/alert_eval.c:1348-1355 | semantic alert rules | qembed | | alert eval | n/a | rule skipped |
| core/alert_sem.c:142-251 | rule description embed | qembed | | detached thread | n/a | rule flagged "not embedded" |
| core/prompt_rank.c, relevance.c, fewshot.c:160-172 | prompt record ranking, relevance verdict, few-shot selection | qembed | | search thread | n/a | inert -> static examples |
| main.c:333,468 | --dispatch, selftest | probe | | CLI | | |
Other: lib/vecindex.c:190 dimension probe via llm_embed; core/embed_pod.c embedding writer; media OCR/phash are external commands (MEDIA_OCR_CMD, MEDIA_PHASH_CMD), not LLM.
Fencing mechanism: prompts.c `prompt_fence_untrusted` (L162): per-call random BEGIN/END token, checked against value so it cannot close the fence; services_list is not fenced (own registry).

**Vector indexes (lib/vecindex.c, vec0 virtual tables, each with *_meta/_done/_failed tables)**
| Index | Spec | Writer | Readers |
|---|---|---|---|
| intel_vec_v2 | embed_pod.c:150 | pod embedding-backfill (120 s, `_maint`) | semsearchapi (/intel/semantic, hybrid, similar), qembed_intel_knn, jp_corpus_lookup, timeline q, alert_eval/alert_sem, story/novelty hooks |
| service_vec | service_vec.c:73 | pod _svec_index (service_vec_pod.c, 900 s) | osint_dispatch routing, /api/sources/semantic, batch_exclusions |
| entity_vec | entity_vec.c:24 | pod entity vec (entity_vec.c:255) | entityapi hybrid search, merge candidates |
| fewshot_vec | fewshot.c:11 | fewshot_sync (vecindex_sync, L143) | fewshot_select in pipeline.c:679 |
| anomaly_vec | anomaly_triage.c (L155) | triage pod | triage memory |
| image_vec | image_vec.c:37 (key sha256) | pod image_vec (IVEC_SID) via JO_VISION_EMBED_URL sidecar | visualsearchapi, camera_refframe, camera_dupes |
| alert_rule_vec | alert_sem.c | alert_sem | alert_eval |

## 4. Pods / background jobs (register via REGISTER_SOURCE; collector `_*` => maintenance lane)
| id | file | interval s | LLM |
|---|---|---|---|
| embedding-backfill | core/embed_pod.c:1017 | 120 | embed (JO_EMBED_URL) |
| _svec_index | core/service_vec_pod.c:65 | 900 | embed |
| entity vec pod | core/entity_vec.c:255 | UNMEASURED | embed |
| image vec pod | core/image_vec.c:1040 | UNMEASURED | vision sidecar |
| camera-stills | core/camera_stills.c:2199 | UNMEASURED | ffmpeg |
| camera-refframe | core/camera_refframe.c:335 | UNMEASURED | no |
| media-analyze (`_enrich`) | core/media.c:2338 | UNMEASURED | ffmpeg/OCR cmds |
| translate (`_enrich`) | core/translate.c:1085 | 600 | chat |
| llm-enricher (`_enrich`) | collectors/pod/llm_enricher.c | 300 | complete (entities+merge judge) |
| anomaly-triage | pod/anomaly_triage.c:533 | 60 | complete+embed |
| collector-repair | pod/collector_repair.c:954 | 120 | complete+embed |
| camera-geocode | pod/camera_geocode_pod.c:321 | 300 | no |
| content-change-retention | pod/content_change_pod.c | 900 | no (table content_snapshots) |
| entity-stats | pod/entity_stats_pod.c | 900 | no (entity_relationships) |
| log-retention | pod/retention_pod.c:278 | 3600 | no (prunes fetch_log, anomalies, repairs, alert deliveries) |
| simhash-backfill | pod/simhash_pod.c | 300 | no (simhash_bands) |
| upload-gc | pod/upload_gc_pod.c | 900 | no |
| wal-checkpoint | pod/wal_checkpoint_pod.c:177 | 300 | no |
| breach-corpus | pod/breach_corpus.c:44 (`_breach`) | 86400 | no |
| geojson-file (`_test`), PORT_SCANNER (`_probe`) | pod/geojson_file.c, sources/port_scanner.c | 0 | no |
Non-scheduler threads: alert_deliver, alert_eval sweep, evidence GC, jwks refresher (section 1). Table read/write detail per pod beyond what is named: UNMEASURED.

## 5. API surface
Routing is a single if-chain in httpd.c handler (not a table): ~151 `eq/starts/seg(u,...)` matchers (routes.txt, httpd.c L2235-5111), plus sub-dispatch inside *api.c. Method checks are inline. Auth gate: pre-auth = /api/health, /api/search/stream/:id (capability UUID, optional bearer), /admin/break-glass/login (rate-limited 5/min/IP); everything else under /api/ passes `auth_check` (httpd.c L2303); operator gates via opgate_check (cadence PUT L2438, source settings PUT L2503, /api/admin/* L3110) returning 403 "Platform operator role required".
| Area | Matchers (httpd.c) |
|---|---|
| search | /api/search/{analyze,suggest,runs,results/:id,stream/:id}, /api/search-history, /api/saved-searches(/:id) |
| intel | /api/intel/{items(+/:uid/... 10),sources,semantic,search}, /api/stories/, /api/timeline, /api/follow/recent |
| sources/collectors | /api/sources{,/search,/stats,/semantic,/categories,/cadence,/:id/{detail,summary,settings,interval,schedule,run,...}}, /api/collectors(/:key), /api/layers(/:id), /api/status(/:id...), /api/admin/sources/:id |
| entities | /api/entities/{stats,search,:id...} |
| alerts/watch | /api/alerts(/preview,/:id), /api/alert-events, /api/watchlists, /api/aoi |
| cases/reports | /api/cases(/:id), /api/annotations, /api/export/*, /api/evidence/{gc,verify,:id}, /api/docmeta, /api/uploads |
| cameras/media | /api/cameras/{visual-search,by-image,:id}, /api/camera-stills/*, /api/data/cameras/{discovery-feed,proxy,run-status,trigger}, /api/media/capabilities |
| geo | /api/geocode(+reverse), /api/isochrone, /api/plateau/tilesets, /api/transit* , /api/data/* |
| breach | /api/breach/search, /api/breach-monitors, /api/admin/breach/{ingest,fetch,catalog/preview,catalog/load,jobs} |
| tenant/identity | /api/me, /api/members, /api/keys, /api/tenant-keys, /api/audit, /api/audit/verify, /api/permalink, /api/vocab |
| admin (operator) | /api/admin/{thresholds,restart,maintenance(+lists,source),repairs,anomalies,cameras,translate/backfill,layers/review,ffmpeg/capabilities,pagination/{probe,jobs}}, /api/db/{tables,scheduler} |
Exact route+method count UNMEASURED beyond ~151 matchers; per-route role detail partly in sub-dispatchers (casesapi viewer/analyst roles per CLAUDE.md).

## 6. Database
SQLite (third_party/sqlite3.c amalgamation) with: WAL (`PRAGMA journal_mode=WAL`, synchronous=NORMAL, busy_timeout 5000, foreign_keys ON; db.c:389-392), FTS5 (intel_items_fts, entities_fts, breach_fts, gtfs_rt_alerts_fts, each with *_uid_map; MeCab-segmented text), sqlite-vec vec0 (auto-extension, db.c:385; tables created in vecindex.c:345). ~149 CREATE TABLE sites. Grouped:
- Core corpus: intel_items, intel_items_fts(+uid_map), sources, source_sched_state, source_settings, source_summary, source_interval_override, fetch_log, collector_cache, collector_ttls, collector_url_overrides, simhash_bands, item_views, _fts_meta, _fts_rebuild, _registry_seed
- Ops/health: collector_anomaly, collector_repair, content_snapshots, settings_thresholds, app_settings, translate_state, entity_extraction_state, entity_resolve_state, entity_norm_state
- Entities/stories: entities(+fts), entity_mentions, entity_merges, entity_relationships, llm_station_merges, story_meta, story_pending
- Alerts: alert_rules, alert_events, alert_deliveries, alert_eval_cursor, alert_rule_vec, watchlists, areas_of_interest
- Tenancy/auth: tenants, users, memberships, tenant_api_keys, tenant_idp_connections, tenant_invites, tenant_quotas, tenant_secrets, tenant_report_templates, sso_group_role_map, audit_events (hash chain)
- Cases/search: cases, case_items, case_members, case_activity, annotations, saved_searches, search_history, search_run_owners, evidence, uploads, upload_parts
- Media/camera: media_assets, media_state, camera_stills(_state), camera_refframes, image_vec_ref
- Transit: gtfs_* (14 tables), gtfs_rt_*, odpt_station_timetable(+fetched), station_clusters, station_footprints, station_line_dots
- Breach: breach_items, breach_fts, breach_meta, breach_monitors, breach_monitor_domains
- Vector (vec0 + _meta/_done/_failed): intel_vec_v2, service_vec, entity_vec, image_vec, fewshot_vec, anomaly_vec

## 7. Dependencies and env
Linked (Makefile L6-75): libcurl, OpenSSL, MeCab (mecab-config), zlib, pthread, libm, libdl (+iconv off-Linux). Vendored third_party: sqlite3, sqlite-vec, mongoose, cJSON. External processes: ffmpeg/ffprobe (media.c, camera_stills.c, ffmpeg.c via netjail), OCR/phash commands (MEDIA_OCR_CMD, MEDIA_PHASH_CMD), llama-server (generation :8080, suggest :8081, embed :8082), optional vision embedding sidecar. 
Env: 185 distinct `"JO_*"` string literals in core/lib/collectors/main; 115 distinct names read via direct getenv across JO_*/LLM_*/embed (list with files: scratchpad/env.txt), plus 44 helper-wrapped env_int/env_long("JO_...") sites; also PORT, MEDIA_*, LLM_*. Reads are scattered module-locally (each file has its own env_long/env_int copy: embed_pod.c, translate.c, llm_worker.c, ...).

## 8. Data pipeline end to end
1. Collector `run(ctx)` (hp table engine lib/hpengine.c, VJSON/VGEO lib/jsonlist.c, bespoke collectors) called by `scheduler_run_source` (scheduler.c:761) with an `intel_sink` wrapped by count_sink; hostgate/httpclient for fetch.
2. `emit()` (core/intel.c:502): uid make_uid, upsert on remote_key, content hash, fts_write (FTS5 + MeCab), geo; commit then simhash segment fingerprint (simhash.c) and `alert_eval_on_item` (intel.c:668, rule match at ingest). Notices/fetch-failure records stored by sink and scheduler.
3. Run line `records= stored= bytes= requests=` -> fetch_log, sources.status (sources_row_record).
4. Async pods (maintenance lane): simhash-backfill, translate (-> `keywords` FTS), llm-enricher (entities, merges), embedding-backfill (-> intel_vec_v2 then embed_hook_register hooks: stories/novelty, semantic alerts), entity-stats, content-change, media-analyze, camera-stills, anomaly-triage/collector-repair on collector_anomaly.
5. alert_eval_sweep thread reconciles entity watchlists after extraction; alert_deliver thread sends events (UNIQUE rule,item).
6. User-initiated OSINT search: searchapi run_thread -> `osint_pipeline_run` (pipeline.c:630): fewshot_select -> analysis LLM -> run_tasks (dispatch_worker threads -> osint_dispatch -> entity pivots) -> phase-2 rounds -> synthesis; progress via progress.c -> SSE.
NOTE: core/pipeline.c is the search-run orchestrator, NOT an ingest-stage pipeline; ingest "stages" are the sink + independent pods polling tables.
7. Serving: httpd -> intelapi/semsearchapi/entityapi/... -> JSON; clients (client/src/api, iOS) consume REST + SSE.

## 9. Architecture smells / bad choices (evidence)
- **httpd.c monolith** 5,245 lines; routes are a hand-rolled if-chain of ~151 matchers with duplicated body-malloc boilerplate (`bdy = malloc(...)` repeated; L2313..), duplicate `/api/db/tables` and `/api/db/scheduler` matchers (L3111-3112 and L3556-3559), duplicated tenant-keys/audit matchers (L4625-4652, 4707-4722): there is no route table, so no way to enumerate auth gate/method per route.
- **Thread-per-request off-loop with a hard cap of 16** (WBODY_SLOTS, httpd.c:760) tied to a reply-parking table; overflow returns 503. Capacity is coupled to a buffer-eviction detail, and ~12 handlers each hand-roll create/detach/wakeup.
- **Event loop blocking risk**: inline handlers (e.g. /api/sources/*, /api/intel/items, db explorer, status) run SQLite on the loop thread; CLAUDE.md itself records stalls ("no /api/health, every SSE stream stalled", httpd.c:1796). UNMEASURED per route.
- **Env-var sprawl**: 185 JO_* literals, per-file private env_long/env_int helpers (translate.c, embed_pod.c, llm_worker.c, scheduler.c...), no central registry except thresholds.c for 11 similarity keys.
- **Duplicated mechanisms**: three LLM entry paths (llm_init interactive, llm_init_suggest, hand-built `llm_client{...}` in qembed.c) plus direct llm_embed calls in 5 files bypassing qembed (semsearchapi:226, service_vec:806, anomaly_triage:275, collector_repair:462, vecindex:190); `LLM_*` vs `JO_*` naming split; GBNF-grammar `llm_complete` path and JSON-schema `llm_chat` path both live.
- **Single LLM worker thread per base_url**: all background pods (translate, enrich, triage, repair) and interactive search share one serialized server; only lane priority protects interactivity (llm_worker.c:114).
- **Detached, unjoinable workers + `_exit`** on shutdown (main.c ~L450) to dodge OpenSSL teardown UAF; many detached threads touch shared DB handle (pagination_probe falls back to the shared handle, httpd.c comment L5215).
- **Collector label overloaded as scheduling lane** (`collector[0]=='_'` => maint lane, scheduler.c:1083) and as pivot reachability (registry NEVER-RUNS rule): behaviour encoded in a string prefix.
- **Pipeline naming confusion**: pipeline.c = search orchestrator; ingest has no staged pipeline, only table-polling pods, so ordering/latency between emit and enrichment is implicit (each pod has its own watermark table).
- **Embedded 60k+-source registry compiled in** (registry.c MAX_SOURCES 131,072; source_registry.gen.c) -> 1,860-object link, CI heavy (CLAUDE.md).
- **No server-side websocket; SSE only** for progress (fine, but search stream is pre-auth capability-UUID).
