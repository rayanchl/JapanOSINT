# JapanOSINT collectors/sources architecture (measured 2026-11-14, tree at /home/user/JapanOSINT/native)

Method: grep over the tree plus the built binary (`JO_DB=<scratch> ./bin/japanosint --list-sources`, `--collector-census`). CLAUDE.md numbers are STALE (it says 61,527 registered; binary says 85,767; it only knows batches up to 57, tree has batch 58-63 docs).

## 1. Acquisition ABI
| item | fact (file) |
|---|---|
| source_def | source.h:124. Fields: id, collector, name, name_ja, update_interval_sec, run(ctx,sink), category, type, url, description, license, layer, free_tier, engine, adaptive_interval |
| source_ctx | source.h:64. source_id, config_json, entity, entity_type, http, llm, db, tenant_id, cancel, progress, resume_in, resume_out (page_resume cycle token), body_ceiling |
| intel_item | source.h:16. uid, remote_key, title, body, summary, link, author, lang, published_at, lat/lon, geometry_geojson, record_type, sub_source_id, properties_json, tags_json |
| intel_sink | source.h:46. ctx + emit(); returns 1 new / 0 update / <0 error. ONE chokepoint (core/intel.c) |
| polymorphism | no scheduled/pivot kind: ctx->entity != NULL means pivot run |
| REGISTER_SOURCE | source.h:229, __attribute__((constructor)) -> registry_add(&def) |
| registry.c | MAX_SOURCES 131072 (static array 1 MB .bss, l.38); FNV-1a open-addressing id index sized 2*MAX (l.136-141, built once via pthread_once in registry_freeze); DUPLICATE id report (l.125), NEVER-RUNS report (l.150, source_never_runs l.96), OVERFLOW drop (l.169); registry_is_entity_pivot (l.199): interval==0 && collector not "_*" && (engine=="hpengine" ? hp_source_uses_entity(id) : true) |
| Binary today | 85,767 registered; 0 DUPLICATE, 0 NEVER-RUNS, 0 OVERFLOW; 83,530 scheduled (interval>0), 2,237 interval 0; 21 "_" collectors |

## 2. Ways a source is defined (counts: files / registrations)
Total registry 85,767. Counting method: `^\s*MACRO(` invocations / `{ .id =` rows.
| mechanism | files | registrations | where |
|---|---|---|---|
| hpengine tables (`HP_REGISTER_TABLE`, lib/hpengine.h:583) | 387 files (263 in collectors/pivot/table, 123 in collectors/feed/generated/hp*, 1 collectors/, 1 core/, 1 lib/ = decl) | 71,423 rows in pivot/table + 337 in generated hp* = ~71,760 `{ .id =` rows | each row -> hp_register() builds a source_def with run=hp_run, engine="hpengine" (hpengine.c:6171) |
| VJSON | 279 | 4,424 | collectors/sources/_verified_macros.inc:167 (jsonlist_emit_paged) |
| VRSS | 104 | 1,907 | _verified_macros.inc:20 (rss_atom) |
| VGEO | 55 | 763 | _verified_macros.inc:134 (geojson_emit_paged) |
| VJSON_KEYED | 30 | 114 | _verified_macros.inc:188 (one idfield) |
| VCSV | 19 | 135 | _verified_macros.inc:247 |
| VJSON_PREP | 26 | 62 | _vjson_idkeys.inc:180 (C prep fn reshapes page) |
| VJSON_IDKEYS | 16 | 40 | _vjson_idkeys.inc:143 (composite `+`/`,` keys) |
| VNCBI | 3 | 13 | _ncbi_esummary.inc:154 |
| VJSONBIG | 1 | 5 | _verified_macros.inc:216 (streamed big) |
| VGEO_IDKEYS | 4 | 5 | _vjson_idkeys.inc:161 |
| DEFR / DEF | 4 / 3 | 17 / 16 | _source_macros.inc:50/57 (hand run(); DEF = pivot stub; label "osint_"+CAT) |
| hand-written REGISTER_SOURCE | collectors/sources: 973 files; 1,262 REGISTER_SOURCE lines tree-wide in 1,044 files | ~1,108 `.id =` lines in collectors/sources (+82 in generated gnews/world/vsrc) | collectors/sources/*.c; collectors/feed/handwritten has 1 file (ua_bulk_registers.c, streamed bulk) |
| generated feed files | collectors/feed/generated: 462 files = 324 vsrc*, 123 hp*, 11 world*, 4 gnews* | | rendered from manifests by collectors/gen_verified_sources.py; wiped on regeneration |
Residual: the sum of the above is ~80.4k vs 85,767; ~5k not attributable by grep (UNMEASURED; probably macro-in-macro/loop forms in .inc files like od_shared.inc, sanc_common.inc, trn_common.inc, av_common.inc). Source-file totals: collectors/sources has 1,022 entries (973 .c + .inc/.h).
Include counts of macro headers: `_verified_macros.inc` included by 327 files, `_vjson_idkeys.inc` 28, `_source_macros.inc` 39.

### Pods (collector key starting "_" or in _maint/_enrich/_breach/_test/_probe lanes). 22 registered rows with `_`-prefixed collector (21 from list + PORT_SCANNER), the engine does not run them via intel feeds; they write own tables.
| id | collector | interval s | file | purpose |
|---|---|---|---|---|
| embedding-backfill | _maint | 120 | core/embed_pod.c | embed new/changed intel rows into intel_vec_v2 |
| _entity_vec | _maint | 600 | core/entity_vec.c | entity name/romaji vector index |
| _image_embed | _maint | 600 | core/image_vec.c | image vector index |
| _svec_index | _maint | 900 | core/service_vec_pod.c | embed every source card (service_vec) |
| _camera_refframe | _maint | 3600 | core/camera_refframe.c | camera reference frames |
| camera-stills | _maint | 300 | core/camera_stills.c | ffmpeg still capture lane |
| camera-geocode | _maint | 300 | collectors/pod/camera_geocode_pod.c | geocode cameras |
| translate | _enrich | 600 | core/translate.c | LLM translation |
| media-analyze | _enrich | 900 | core/media.c | media asset analysis |
| llm-enricher | _enrich | 300 | collectors/pod/llm_enricher.c | LLM enrichment |
| anomaly-triage | _maint | 60 | collectors/pod/anomaly_triage.c | triage collector anomalies |
| collector-repair | _maint | 120 | collectors/pod/collector_repair.c | repair proposals |
| simhash-backfill | _maint | 300 | collectors/pod/simhash_pod.c | simhash fingerprint backfill |
| content-change-retention | _maint | 900 | collectors/pod/content_change_pod.c | content-change pruning |
| entity-stats | _maint | 900 | collectors/pod/entity_stats_pod.c | entity PMI/co-occurrence stats |
| log-retention | _maint | 3600 | collectors/pod/retention_pod.c | prune fetch_log, anomalies, alerts in batches |
| upload-gc | _maint | 900 | collectors/pod/upload_gc_pod.c | uploaded-file GC |
| wal-checkpoint | _maint | 300 | collectors/pod/wal_checkpoint_pod.c | SQLite WAL checkpoint |
| breach-corpus | _breach | 86400 | collectors/pod/breach_corpus.c | breach corpus catalogue |
| geojson-file | _test | 0 | collectors/pod/geojson_file.c | test fixture in production tree |
| PORT_SCANNER | _probe | 0 | collectors/sources/port_scanner.c | raw sockets; "_probe" keeps it out of LLM schema |
Registration sites: 12 files in collectors/pod, 18 files in core/ reference REGISTER_SOURCE (pods in embed_pod, image_vec, entity_vec, camera_stills, camera_refframe, media, translate, service_vec_pod, db.c).

## 3. hpengine (lib/hpengine.h 729 lines, lib/hpengine.c 6,397 lines)
Modes (`hp_mode`): HP_JSON 0, HP_HTML 1, HP_CSV 2, HP_XML 3, HP_XLSX 4, HP_RECJAR 5 (IANA record-jar), HP_NDJSON 6. (HTML table sub-mode via `table_must`: hp_run_table, hpengine.c:3895.) `hp_want` shape gates: ANY, DOMAIN, IP, EMAIL, NUMERIC, HASH, ICAO24, ETH, BTC, ASN.
hp_source has ~85 fields. Grouped, with grep'd row counts (lines `.field =` in collectors/):
| capability | fields | rows using |
|---|---|---|
| identity/meta | id, name, name_ja, collector, category, type, portal, description, record_type, tags, lang, layer | all |
| request | url, post_body (53), content_type, headers[5] (618), key_env (167 rows / 74 distinct env vars), timeout_ms, tz_minutes | |
| record extraction | array_path, title_keys, id_keys (64,621), link_keys/link_tmpl, date_keys, body_keys, lat_key/lon_key, latlon_key/lonlat_key, row_header_path (JSON table w/ header array) | |
| identity extras | occurrence_rows (85), date_field | |
| paging | page_param (34,859), page_start, page_zero_based, page_size, page_max, next_path (563, supports `$last` and `k=v`), next_tmpl, page_walk (295, delegates to jsonlist_next_page), page_delay_ms | |
| cycling | page_resume (19,666), window_days + window_from (49; `{wstart}`/`{wend}`), adaptive_interval (25,846), interval | |
| detail hops | detail_url (619), detail_key, detail_path, detail_max, detail_html_kv; budget hp_detail_admit 1,000 hops / 120 s | |
| containers | xlsx_sheet / xlsx_sheet_index / xlsx_sheet_all (1), zip_all_entries (1), index_url + index_href_must (15) | |
| CSV | csv_delim, csv_comment, csv_no_header, csv_skip_lines, charset | |
| HTML | href_must, base, table_must, table_index, table_skip_rows | |
| date tokens | {date}, {date±Nd}, {date:FMT:±N}, {ago:N}, {wstart}/{wend}; date_back (55), date_step | lib/datetok.c |
| entity tokens | {q},{Q},{qd},{qc},{qh},{qu},{ql},{qU},{qn},{key},{keyb64},{v},{page},{offset},{size} | |
| filter | filter_query, max_items, want | |

## 4. Shared libraries (native/lib) — direct `#include` counts (core+collectors), LOC of .c
| lib | LOC | what | collector files incl. |
|---|---|---|---|
| hpengine | 6,397 | declarative record engine | 387 (tables) |
| jsonlist | 2,163 | VJSON array->items, jsonlist_emit_paged, next-page decision | via _verified_macros.inc (327 files) + hpengine |
| geojson | 703 | GeoJSON/ArcGIS features, geojson_emit_paged | 211 |
| feedlib | 192 | feed_get_json/text, HTTP wrappers | 436 |
| rss_atom | 711 | RSS/Atom parse (dead-feed = failed fetch) | 42 (+macros VRSS) |
| pagewalk | 666 | pw_walk/pw_walk_max, jo_page_max(_for), declared-total | 45 (+idkeys.inc) |
| csv | 619 | CSV parser | 19 (+VCSV, hp) |
| xlsx | 598 | worksheet->CSV | hpengine only |
| zipread | 221 | zip central dir, named/indexed entries | 3 + xlsx + hpengine |
| htmlparse | 770 | anchor scanner, html_table_parse | 34 |
| datetok | 227 | date tokens | 1 (.inc) + hp |
| seenset | 84 | growing dedupe set | 13 |
| jsonstream | 333 | streamed big JSON (own libcurl handle) | 1 (.inc) |
| overpass | 456 | Overpass QL fetch | 98 |
| mlit_ksj | 251 | MLIT Kokudo Suuchi | 6 |
| keyqual | 150 | key qualification | 11 |
| threatintel | 96 | threat feed helpers | 20 |
| unified | 247 | unified map layer | 9 |
| linecolor | 104 | rail line colours | 8 |
| arcgis_dir | 78 | ArcGIS directory walk | 8 |
| osintemit | header | emit helpers | 11 + jsonlist, hpengine |
| probe | 28 | | 7 |
| camfeature | 95 | camera features | 3 |
| sgp4 | 314 | satellite propagation | 1 (satellite_tracking.c) |
| ws | 90 | websocket client (own curl handle) | 2 (bluesky_jetstream_jp.c, certstream_jp.c) |
| vecindex | 1,473 | vec0 KNN lib | 0 collectors, 19 core files |
| bloom, bigfile | 140, 50 | | core/breach_index.c only |
| jpnorm, utf8 | 330 | Japanese normalisation | core (fts, entitystore, relevance, suggest) only |
| jocore.h | header | jo_strcasestr etc. | 344 |

## 5. Special features and unusual dependencies
- Raw sockets / net: port_scanner.c (_probe), dns_records.c, ssl_analyzer.c, shodan_search.c, email_validator.c, ip_reputation.c, reverse_dns.c, tor_exit_check.c, soc_supplychain.c; core/netjail.c, core/hostgate.c.
- ffmpeg/camera: core/ffmpeg.c, netjail.c (per-child forward proxy, netns), camera_stills.c, hls_check.c, media.c, httpd.c (popen/exec 6 files, all core). Collectors: public_cameras.c, shodan_cameras_jp.c, cam_curated_jp.c use lib/camfeature.
- Websocket: certstream-jp (60 s), bluesky-jetstream-jp (600 s) via lib/ws.c.
- SGP4: satellite-tracking (collector "satellite").
- Overpass: ~98 collector files (fire_station_map, lighthouse_map, wineries_craftbeer ...).
- MeCab: only core/fts.c (not collectors).
- SQLite direct in collectors: 33 collector files (88 files tree-wide incl. pods): sources/ transport_cluster_runner.c, twitter_geo.c, wayback_jp.c (cycle by hand), jp_corpus_lookup.c, gtfs_jp.c; all pods.
- Threads: pthread_create only in core + username_osint.c (collectors).
- Credentials: 167 `key_env` rows over 74 distinct env vars; top: OPENALEX_API_KEY 58, FEC_API_KEY 7, CONGRESS_GOV_API_KEY 7, GBIZINFO_API_TOKEN 6, EIA_API_KEY 4, then 3x UK_CHARITY_KEY/SAM_API_KEY/REGULATIONS_GOV_API_KEY/FRED_API_KEY/DART_API_KEY. Bespoke getenv in collectors: 167 distinct names (GITHUB_TOKEN x8, ESTAT_APP_ID x7, SHODAN_API_KEY x5, ODPT_* x12). Missing key -> collector-status-notice (_credential_notice.inc, lib/osintemit.h).
- POST bodies: 53 rows. Custom headers: 618 rows. Detail hops: 619 rows.
- host_agents.def (core/host_agents.def): 6 agent kinds; HOST_AGENT host lines: HA_COMPAT 321, HA_NOCOLLECTOR 65, HA_BROWSER 15, HA_CONTACT 6, HA_RCORP 6, HA_SEC 3 = 416 hosts. Resolved in one function host_agent_ua() (httpclient.c:386), read also by tools/probe_hp_batch.py; `--host-agents` prints it. $SEC_CONTACT_UA overrides SEC.
- hostgate per-host gaps (core/hostgate.c:95-106, HG_MAX_OVERRIDES 32): 8 builtins: reddit.com 30000 ms, news.google.com 1500, eutils.ncbi.nlm.nih.gov 400, crates.io 1100, mobile311.sfgov.org 1200, data.qld.gov.au 1500, data.nsw.gov.au 250, apilist.tronscanapi.com 1000; plus env JO_HOST_MIN_GAP_OVERRIDES. Also per-host capacity, Retry-After, SSRF/camera policy (hostgate_url_check, hostgate_addr_check_floor).
- TLS: core/catrust.c + data/tls/extra-intermediates.pem (7 PEM certs, intermediates only; installed per handle via CURLOPT_SSL_CTX_FUNCTION, httpclient.c:214); http_last_tls_cause() names 9 causes.
- Breach corpus: collectors/pod/breach_corpus.c, core/breach_index.c (bloom+bigfile), core/breach_jobs.c.
- Wayback: collectors/sources/wayback_jp.c (hand-rolled cycle, predecessor of page_resume).
- third_party/: cJSON, mongoose (HTTP server, 1.1 MB), sqlite3 amalgamation (8.9 MB), sqlite-vec (+licences).
- LegCo HK: plain http deliberately (TLS unknown-CA in this container) per CLAUDE.md; UNMEASURED by me.

## 6. Run paths
Scheduled run: scheduler thread (core/scheduler.c, sched_thread ~l.1648; worker_thread l.1228; due time via sched_row_interval l.280 = per-source override else cadence_apply(sched_state_base)) -> scheduler_run_source (l.763): intel_sink_make (core/intel.c:694) wrapped in count_sink; http_client_new; source_ctx with resume_in via sched_resume_load / resume_out=sched_resume_out (scheduled only), cancel slot -> `d->run(&ctx,&sink)` -> for hp rows hp_run (hpengine.c:4555; hp_lookup(ctx->source_id) -> spec; mode driver hp_run_json/csv/xml/table/html/ndjson/recjar; page walk via jsonlist_next_page/pw; hp_deepen detail hops gated by hp_detail_admit) -> http_request (core/httpclient.c:781; host_agent_ua, hostgate_acquire/release/penalize, hostgate_url_check + per-connection hostgate_addr_check_floor, catrust_install, retry loop with Retry-After, tls cause) -> emit -> count_emit -> intel emit (intel.c:502): make_uid (explicit uid else source|remote_key, SHA-256 suffix for long keys, l.401), content_hash, one indexed probe to tell insert vs update (new vs refetched), UPSERT (SQL_UPSERT), fts_write (MeCab via core/fts.c), alert_eval_on_item only on NEW, simhash_on_item_segmented, distinct-uid counting for `stored=`. After run: hosts/hosts_ok/bytes/requests from http_client; run_status(); sched_disclose_fetch_failure (scheduler.c:626) for collectors that did not self-disclose (two triggers); fetch_log row; sources_row_record updates sources.status; sched_freshness_measure (l.294) for adaptive rows.
Entity pivot: API/LLM -> osint_dispatch (core/osint_dispatch.c:750/756; dispatch_impl l.762; resolves id via osint_canon / osint_resolve_near / embedding resolve; is_entity_pivot l.317 delegates to registry_is_entity_pivot) -> source_ctx with ctx.entity/entity_type (l.835-844), no resume tokens -> same run()/http/engine -> dual_emit (l.679) captures records for the LLM and stores to sink.
Notices (record_type; written in lib/hpengine.c l.5884/6032/6080 + shape notices; scheduler.c; _credential_notice.inc; lib/osintemit.h; threatintel.c):
- collector-fetch-failure: hpengine (upsert-keyed per source+entity, l.6032) AND scheduler sched_disclose_fetch_failure for every non-hp collector.
- collector-truncation-notice: hpengine l.5884/6080, jsonlist, pagewalk, geojson (ceiling, cut page, detail budget, malformed NDJSON lines, zip/xlsx members).
- collector-shape-notice: hpengine (once per run per condition, e.g. id-keys-unmatched, anchors-unlabelled), jsonlist (label-keys-unmatched).
- collector-status-notice: key-gated rows needing env var (_credential_notice.inc), cyi_pdns_mnemonic.c, osintemit.h.
Notices counted separately from records (intel_item_is_notice, intel.c:128; run line `notices=N`).

## 7. Collector grouping
core/collectors.def: 678 COLLECTOR() entries = 24 groups (grp_*, parent NULL) + 654 collector keys; census (`--collector-census`) lists 654 keys over 85,767 sources; 0 unassigned (`grp_unassigned` exists as fallback). Generated by tools/gen_collectors_def.py (610 lines; OVERRIDES, KEYWORDS, CATEGORY_GROUP) from the binary's census; `make collectors-def` / `collectors-def-check`.
Group: collector keys / sources
grp_government 137 / 28,358; grp_geo 24 / 11,360; grp_infrastructure 29 / 7,527; grp_media 27 / 7,492; grp_corporate 77 / 5,106; grp_health 30 / 3,921; grp_education 4 / 3,893; grp_opendata 66 / 3,821; grp_environment 37 / 2,982; grp_science 26 / 2,537; grp_legal 32 / 1,941; grp_culture 11 / 1,543; grp_cyber 21 / 1,321; grp_social 10 / 926; grp_safety 18 / 901; grp_transport 18 / 757; grp_surveillance 8 / 313; grp_osint 10 / 322; grp_telecom 9 / 270; grp_space 4 / 208; grp_procurement 48 / 187; grp_sanctions 4 / 61; grp_internal 4 / 20.
Largest single keys: frgov_mairies 6,442; hp3b62_eu 4,535; hp3b61_us 3,908; hp3b63_cn 2,870; itgov_scuole 2,725; hp3b59_jpgis 2,543; hp3b60_us 2,495; hp3b44_usgov 2,072.
Collector key for hp rows defaults to table file basename (hpengine.c hp_register dflt); explicit `.collector` overrides it.

## Architecture smells (file:line)
1. Four paging engines with overlapping decisions: jsonlist_emit_paged (lib/jsonlist.c:1734) / jsonlist_emit_paged_keyed (:2130), geojson_emit_paged (lib/geojson.c:516), pw_walk (lib/pagewalk.c:416), plus hpengine's own walk (page_walk delegates only the JSON next-page decision). CLAUDE.md records fixes landing in one copy and not others.
2. Three declared-total key lists: lib/jsonlist.c:1409-1415, lib/pagewalk.c:169-170 and :213, lib/hpengine.c:3133 (XML). Must be edited together.
3. Page ceiling knob split: jo_page_max()/jo_page_max_for() in lib/pagewalk.c:38/65 but legacy $JO_JSONLIST_PAGE_MAX still read; hpengine had its own constant until recently.
4. Two curl bypasses of http_request policy: lib/ws.c:19 (curl_easy_init, no hostgate/agent table check found in file) and lib/jsonstream.c:244 (re-implements hostgate checks, l.230); also core camera/hls paths. Any new http policy must be copied.
5. Three per-host policy tables: core/host_agents.def (416 hosts), core/hostgate.c:95 gaps (8), plus per-row `.headers` (618) still on rows; also TLS special-casing in catrust. No single "host profile".
6. Registry-side coupling upward: registry.c:197 `#include "lib/hpengine.h"` to call hp_source_uses_entity; and osint_dispatch.c:317 keeps a second static is_entity_pivot wrapper.
7. Seen-set duplicated: lib/seenset.c (84 LOC, used by 13 collectors) vs hand-rolled seen_put/seen_add hash in core/intel.c:141-154.
8. Pod registration is scattered and typed by string prefix: 12 files in collectors/pod, ~9 in core/, key "_" prefix convention in registry.c/scheduler.c; "geojson-file" (_test) ships in production registry.
9. Source metadata overlay: core/source_registry.gen.c (378 lines) + source_registry_dyn.c overlay Node-era prose over source_def (source.h comment says new sources must not be added) - two metadata stores.
10. Registered-count drift: CLAUDE.md quotes 61,527 (and floors in tools/registry-floor.txt / source-floor.txt); binary reports 85,767; docs for batches 58-63 exist but the text stops at 57. Floors are the gate; prose is stale.
11. Generator-owned vs hand-owned files in the same dir: collectors/feed/generated is wiped on regeneration (ua_bulk_registers.c had to live in feed/handwritten); hp tables are "C is maintained copy, manifest is record" - two sources of truth, opts table in gen_hp_batch.py is a third place to update per new hp_source field.
12. hp_source is a ~85-field flat struct (lib/hpengine.h) where each capability (paging, windows, containers, detail hops) is orthogonal fields rather than sub-structs; hpengine.c is 6,397 lines in one file.
13. engine tag is a free string ("hpengine","vjson","vrss","vgeo","vcsv","socrata","rss",NULL) and nothing dispatches on it except registry_is_entity_pivot comparing to "hpengine".
14. UNMEASURED: exact per-engine split of the 85,767 (≈5k unattributed), and `.engine` value counts (binary list does not expose it; --list-sources prints only id/collector/interval).
