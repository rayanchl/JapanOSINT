/* core/dataapi.c — see dataapi.h.
 *
 * Faithful port of server/src/routes/data.js → respondWithData() general
 * path, under the unified C source ABI. data.js is ~1843 lines but most of it
 * is (a) ~50 thin per-id router.get() wrappers that all call respondWithData
 * with {sourceId, layerType, collectorKey} (collapsed here: under the unified
 * ABI collectorKey == sourceId == the registry id) and (b) a handful of
 * per-collector special-case routes. The reusable core is respondWithData.
 *
 * REPRODUCED faithfully:
 *  - registry_get(id) lookup → run() through an in-memory capture intel_sink,
 *    reconstructing each emitted item's Feature from geometry_geojson +
 *    properties_json (identical to lib/unified.c's cap_emit).
 *  - The canonical envelope { type:'FeatureCollection', features, _meta:{...} }
 *    with the exact normaliseFc()/buildFcFromIntel() _meta keys: source,
 *    fetchedAt, recordCount, live, description (+ cache_status, age_ms,
 *    ttl_ms, served_from).
 *  - The empty-FC-when-no-collector graceful path: an unknown collector that
 *    still has intel_items rows serves those (buildFcFromIntel fallback); a
 *    truly registered-but-empty source returns the explicit empty FC with
 *    description "No collector registered for this source." — NEVER a 404.
 *  - The intel-envelope branch: a collector that emits only non-geo items
 *    (data.kind === 'intel') yields the migrated-to-intel empty FC with
 *    migrated_to_intel / intel_endpoint meta.
 *  - The collector_cache + collector_ttls TTL semantics: SQLite-backed,
 *    fetched_at + ttl_ms expiry, ON CONFLICT upsert, TTL clamp
 *    [MIN,MAX] with DEFAULT_TTL fallback — a direct port of
 *    utils/collectorCache.js. Cache HIT short-circuits the run and serves the
 *    intel_items reconstruction (cache_status:'hit'), exactly as data.js does.
 *  - selectGeoFeatures() row→Feature shaping (the buildFcFromIntel source):
 *    the SELECT column set, the default `lat IS NOT NULL` filter, parsed
 *    properties spread + the fixed overlay keys in JS order, geometry from
 *    the geometry column else a Point fallback from lat/lon.
 *
 * DOCUMENTED DEVIATIONS (subsystem absent in C scope / out of this port):
 *  - layer_work_* telemetry (broadcastLayerWorkStarted/Finished →
 *    utils/layerEvents.js → the WS broadcaster) is a NO-OP stub here: it
 *    needs the websocket broadcaster subsystem, which is not in scope for
 *    this file. See lw_started()/lw_finished() below — call sites are kept so
 *    wiring it later is mechanical.
 *  - The ?at=<iso>&window=<sec> time-slider replay short-circuit
 *    (getTemporalForLayer + emptyReplayFc + the windowed selectGeoFeatures
 *    branch) is not ported: the C HTTP layer does not thread query params to
 *    this entrypoint (signature is (db,id) only). The live path is the
 *    general path; replay is the documented follow-up.
 *  - mirrorCollectorOutput (utils/collectorMirror.js): in the unified C ABI
 *    every source already emits straight into the real intel_sink on its
 *    scheduled/explicit runs (core/intel.c), so intel_items IS the master.
 *    Here we run the collector through a capture sink to BUILD the response
 *    (matching normaliseFc's collector output), and additionally serve the
 *    intel_items reconstruction when rows already exist — same observable
 *    result as data.js's "prefer intel_items, fall back to collector FC".
 *    We deliberately do not double-write intel_items from this read path.
 *  - withCollectorRun / annotateLastHit (utils/collectorTap.js — last-hit
 *    bookkeeping for the SourcesPanel) is not ported: separate subsystem.
 *  - Per-collector special-case routes in data.js (camera snapshot capture &
 *    allowlist, /cameras/discovery-feed, MLIT dam-live, flights ADSB
 *    snapshot poller, OpenSky OAuth, flight-enrich cache) are NOT general
 *    respondWithData paths — they are bespoke handlers and out of scope for
 *    this generic endpoint. The general collector path is ported.
 */
#include "dataapi.h"
#include "../source.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* ── collectorCache.js constants (verbatim) ─────────────────────────────── */
#define DEFAULT_TTL_MS (15LL * 60 * 1000)        /* 15 min */
#define MIN_TTL_MS     (60LL * 1000)             /* 1 min floor */
#define MAX_TTL_MS     (24LL * 60 * 60 * 1000)   /* 24 h ceiling */

static long long clamp_ttl(long long ms) {       /* == clampTtl() */
  if (ms <= 0) return DEFAULT_TTL_MS;
  if (ms < MIN_TTL_MS) return MIN_TTL_MS;
  if (ms > MAX_TTL_MS) return MAX_TTL_MS;
  return ms;
}

static long long now_ms(void) {
  struct timeval tv; gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void iso_now(char *o, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm g; gmtime_r(&tv.tv_sec, &g);
  snprintf(o, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min,
           g.tm_sec, (int)(tv.tv_usec / 1000));
}

/* getTtlMs(key): collector_ttls row else DEFAULT_TTL_MS. The table is
 * created/seeded by the Node side (collectorCache.js / database.js); we read
 * it if present and tolerate its absence (fresh C-only DB → default). */
static long long get_ttl_ms(db_handle *db, const char *key) {
  sqlite3_stmt *s = NULL;
  long long ttl = DEFAULT_TTL_MS;
  if (sqlite3_prepare_v2(db->h,
        "SELECT ttl_ms FROM collector_ttls WHERE key=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW &&
        sqlite3_column_type(s, 0) != SQLITE_NULL)
      ttl = sqlite3_column_int64(s, 0);
  }
  sqlite3_finalize(s);
  return ttl;
}

/* getCached(key): the cached FC JSON if fresh (ageMs <= ttl_ms), with its
 * stored age. Returns malloc'd JSON or NULL; *age_out set on hit. Mirrors
 * collectorCache.getCached exactly (age = now - fetched_at; expire when
 * age > ttl_ms). Missing table → NULL (miss). */
static char *cache_get(db_handle *db, const char *key, long long *age_out) {
  sqlite3_stmt *s = NULL;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT fc_json, fetched_at, ttl_ms FROM collector_cache "
        "WHERE key=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *fc = (const char *)sqlite3_column_text(s, 0);
      long long fetched = sqlite3_column_int64(s, 1);
      long long ttl = sqlite3_column_int64(s, 2);
      long long age = now_ms() - fetched;
      if (fc && age <= ttl) {
        out = strdup(fc);
        if (age_out) *age_out = age;
      }
    }
  }
  sqlite3_finalize(s);
  return out;
}

/* setCached(key, fc, ttlMs): INSERT … ON CONFLICT upsert, clamped TTL. Best
 * effort — a missing collector_cache table (C-only DB) is silently skipped,
 * exactly like collectorCache catching a stringify failure. */
static void cache_set(db_handle *db, const char *key, const char *fc_json,
                      long long ttl_ms) {
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO collector_cache (key, fc_json, fetched_at, ttl_ms) "
        "VALUES (?1,?2,?3,?4) "
        "ON CONFLICT(key) DO UPDATE SET "
        "  fc_json=excluded.fc_json, fetched_at=excluded.fetched_at, "
        "  ttl_ms=excluded.ttl_ms", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, fc_json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 3, now_ms());
    sqlite3_bind_int64(s, 4, clamp_ttl(ttl_ms));
    sqlite3_step(s);
  }
  sqlite3_finalize(s);
}

/* layer_work_* telemetry — DOCUMENTED NO-OP (see header). Call sites are
 * retained at the same points data.js broadcasts so wiring the WS
 * broadcaster later is a one-liner per call. */
static void lw_started(const char *id) { (void)id; }
static void lw_finished(const char *id, int record_count,
                        const char *cache_status) {
  (void)id; (void)record_count; (void)cache_status;
}

/* ── capture sink (verbatim port of lib/unified.c cap_emit) ─────────────── */
typedef struct { cJSON *arr; } cap_ctx;

static int cap_emit(struct intel_sink *s, const intel_item *it) {
  cap_ctx *c = (cap_ctx *)s->ctx;
  cJSON *g = (it->geometry_geojson && *it->geometry_geojson)
               ? cJSON_Parse(it->geometry_geojson) : NULL;
  cJSON *p = (it->properties_json && *it->properties_json)
               ? cJSON_Parse(it->properties_json) : NULL;
  if (!g || !p) { cJSON_Delete(g); cJSON_Delete(p); return 0; }
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(c->arr, f);
  return 1;
}

/* ── selectGeoFeatures() helpers (port of camera_store.c row shaping) ───── */

/* Move every member of `src` into `dst` (shallow, by reference). Equivalent
 * to the `...properties` object spread; `src` is emptied so the caller's
 * cJSON_Delete(src) is a no-op container free. */
static void spread_into(cJSON *dst, cJSON *src) {
  if (!cJSON_IsObject(src)) return;
  cJSON *child = src->child;
  src->child = NULL;
  while (child) {
    cJSON *next = child->next;
    child->next = child->prev = NULL;
    cJSON_AddItemToObject(dst, child->string, child);
    child = next;
  }
}

/* Overlay key = string|null, replacing any spread-in value (JS literal order:
 * later key wins). */
static void set_str_or_null(cJSON *o, const char *k, const char *v) {
  cJSON_DeleteItemFromObject(o, k);
  if (v) cJSON_AddStringToObject(o, k, v);
  else   cJSON_AddNullToObject(o, k);
}
static void set_item(cJSON *o, const char *k, cJSON *v) {
  cJSON_DeleteItemFromObject(o, k);
  cJSON_AddItemToObject(o, k, v);
}

/* selectGeoFeatures row → Feature. Column order MUST match the SELECT in
 * intel_fc_features() below. Identical shaping to camera_store.c
 * row_to_feature (which is itself the selectGeoFeatures port). */
static cJSON *row_to_feature(sqlite3_stmt *s) {
  /* 0 uid,1 source_id,2 sub_source_id,3 record_type,4 lat,5 lon,6 geometry,
   * 7 title,8 summary,9 link,10 language,11 published_at,12 fetched_at,
   * 13 properties,14 tags */
  const char *c_uid = (const char *)sqlite3_column_text(s, 0);
  const char *c_src = (const char *)sqlite3_column_text(s, 1);
  int subnull = sqlite3_column_type(s, 2) == SQLITE_NULL;
  const char *c_sub = subnull ? NULL : (const char *)sqlite3_column_text(s, 2);
  int rtnull = sqlite3_column_type(s, 3) == SQLITE_NULL;
  const char *c_rt = rtnull ? NULL : (const char *)sqlite3_column_text(s, 3);
  int latnull = sqlite3_column_type(s, 4) == SQLITE_NULL;
  int lonnull = sqlite3_column_type(s, 5) == SQLITE_NULL;
  double lat = sqlite3_column_double(s, 4), lon = sqlite3_column_double(s, 5);
  int gnull = sqlite3_column_type(s, 6) == SQLITE_NULL;
  const char *c_geom = gnull ? NULL : (const char *)sqlite3_column_text(s, 6);
  int tnull = sqlite3_column_type(s, 7) == SQLITE_NULL;
  const char *c_title = tnull ? NULL : (const char *)sqlite3_column_text(s, 7);
  int sumnull = sqlite3_column_type(s, 8) == SQLITE_NULL;
  const char *c_summ = sumnull ? NULL : (const char *)sqlite3_column_text(s, 8);
  int lknull = sqlite3_column_type(s, 9) == SQLITE_NULL;
  const char *c_link = lknull ? NULL : (const char *)sqlite3_column_text(s, 9);
  int lgnull = sqlite3_column_type(s, 10) == SQLITE_NULL;
  const char *c_lang = lgnull ? NULL : (const char *)sqlite3_column_text(s, 10);
  int pbnull = sqlite3_column_type(s, 11) == SQLITE_NULL;
  const char *c_pub = pbnull ? NULL : (const char *)sqlite3_column_text(s, 11);
  int ftnull = sqlite3_column_type(s, 12) == SQLITE_NULL;
  const char *c_fetch = ftnull ? NULL : (const char *)sqlite3_column_text(s, 12);
  int prnull = sqlite3_column_type(s, 13) == SQLITE_NULL;
  const char *c_props = prnull ? NULL : (const char *)sqlite3_column_text(s, 13);
  int tgnull = sqlite3_column_type(s, 14) == SQLITE_NULL;
  const char *c_tags = tgnull ? NULL : (const char *)sqlite3_column_text(s, 14);

  cJSON *props = NULL;
  if (c_props && *c_props) props = cJSON_Parse(c_props);
  if (!props) props = cJSON_CreateObject();
  cJSON *tags = NULL;
  if (c_tags && *c_tags) tags = cJSON_Parse(c_tags);
  if (!tags) tags = cJSON_CreateArray();

  cJSON *geometry = NULL;
  if (c_geom && *c_geom) geometry = cJSON_Parse(c_geom);
  if (!geometry && !latnull && !lonnull) {
    geometry = cJSON_CreateObject();
    cJSON_AddStringToObject(geometry, "type", "Point");
    cJSON *gc = cJSON_CreateArray();
    cJSON_AddItemToArray(gc, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(gc, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(geometry, "coordinates", gc);
  }

  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "type", "Feature");
  cJSON_AddItemToObject(out, "geometry",
                        geometry ? geometry : cJSON_CreateNull());

  cJSON *pr = cJSON_CreateObject();
  spread_into(pr, props);                          /* ...properties */
  set_str_or_null(pr, "uid", c_uid);
  set_str_or_null(pr, "source_id", c_src);
  set_str_or_null(pr, "sub_source_id", c_sub);
  set_str_or_null(pr, "record_type", c_rt);
  set_str_or_null(pr, "title", c_title);
  set_str_or_null(pr, "summary", c_summ);
  set_str_or_null(pr, "link", c_link);
  set_str_or_null(pr, "language", c_lang);
  set_str_or_null(pr, "published_at", c_pub);
  set_str_or_null(pr, "fetched_at", c_fetch);
  set_item(pr, "tags", tags);                      /* tags array (moved in) */
  cJSON_AddItemToObject(out, "properties", pr);

  cJSON_Delete(props);                             /* now an empty shell */
  return out;
}

/* buildFcFromIntel(sourceId): selectGeoFeatures({sourceId}) — features array
 * for source_id=?1 AND lat IS NOT NULL (the includeUngeocoded=false default).
 * Returns the cJSON features array (caller owns it); never NULL (may be
 * empty — caller decides, mirroring `fc.features.length === 0 ⇒ null`). */
static cJSON *intel_fc_features(db_handle *db, const char *source_id) {
  static const char *Q =
    "SELECT uid, source_id, sub_source_id, record_type, lat, lon, geometry,"
    "       title, summary, link, language, published_at, fetched_at,"
    "       properties, tags"
    "  FROM intel_items"
    " WHERE source_id = ?1 AND lat IS NOT NULL";
  cJSON *feats = cJSON_CreateArray();
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, Q, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW)
      cJSON_AddItemToArray(feats, row_to_feature(s));
  }
  sqlite3_finalize(s);
  return feats;
}

/* Does intel_items have ANY row for this source_id? (Used to decide the
 * graceful-empty-FC vs genuine-NULL fall-through for an unregistered id —
 * mirrors data.js serving intel rows that arrived via another path.) */
static int intel_has_source(db_handle *db, const char *source_id) {
  sqlite3_stmt *s = NULL;
  int has = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM intel_items WHERE source_id=?1 LIMIT 1",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
    has = (sqlite3_step(s) == SQLITE_ROW);
  }
  sqlite3_finalize(s);
  return has;
}

/* sources.status for the no-collector envelope's _meta.status (getSourceById
 * → source?.status ?? 'unknown'). Returns malloc'd string or NULL. */
static char *source_status(db_handle *db, const char *id) {
  sqlite3_stmt *s = NULL;
  char *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT status FROM sources WHERE id=?1", -1, &s, NULL)
      == SQLITE_OK) {
    sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *v = (const char *)sqlite3_column_text(s, 0);
      if (v) st = strdup(v);
    }
  }
  sqlite3_finalize(s);
  return st;
}

/* Standard _meta block (normaliseFc/buildFcFromIntel key set + order). */
static void add_meta(cJSON *fc, const char *source, int record_count,
                     int live, const char *description,
                     const char *cache_status, long long age_ms,
                     long long ttl_ms /* <0 → omit */,
                     const char *served_from /* NULL → omit */) {
  cJSON *m = cJSON_AddObjectToObject(fc, "_meta");
  if (source) cJSON_AddStringToObject(m, "source", source);
  else        cJSON_AddNullToObject(m, "source");
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(m, "fetchedAt", ts);
  cJSON_AddNumberToObject(m, "recordCount", record_count);
  cJSON_AddBoolToObject(m, "live", live ? 1 : 0);
  if (description) cJSON_AddStringToObject(m, "description", description);
  else             cJSON_AddNullToObject(m, "description");
  if (cache_status) cJSON_AddStringToObject(m, "cache_status", cache_status);
  cJSON_AddNumberToObject(m, "age_ms", (double)age_ms);
  if (ttl_ms >= 0) cJSON_AddNumberToObject(m, "ttl_ms", (double)ttl_ms);
  if (served_from) cJSON_AddStringToObject(m, "served_from", served_from);
}

/* Build the FeatureCollection from the intel_items reconstruction. Returns
 * malloc'd JSON, or NULL if there are zero geocoded rows (== buildFcFromIntel
 * returning null so the caller can fall back). */
static char *fc_from_intel(db_handle *db, const char *source_id,
                           const char *cache_status, long long age_ms,
                           long long ttl_ms, int *count_out) {
  cJSON *feats = intel_fc_features(db, source_id);
  int n = cJSON_GetArraySize(feats);
  if (n == 0) { cJSON_Delete(feats); return NULL; }
  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON_AddItemToObject(fc, "features", feats);
  /* buildFcFromIntel: live = (at == null) → always true on the live path. */
  add_meta(fc, source_id, n, 1, NULL, cache_status, age_ms, ttl_ms,
           "intel_items");
  if (count_out) *count_out = n;
  char *js = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return js;
}

char *dataapi_layer(db_handle *db, const char *id) {
  if (!db || !id || !*id) return NULL;

  /* In the unified ABI collectorKey == sourceId == the registry id (data.js
   * passed all three; they coincide here). */
  const source_def *def = registry_get(id);

  /* ── No collector registered ───────────────────────────────────────────
   * data.js: serve intel_items rows that landed via another path; else the
   * explicit "No collector registered" empty FC (NOT a 404). We additionally
   * return NULL (→ caller falls through to 501) only when the id is genuinely
   * unknown AND has no rows — that's the dataapi.h contract and the closest
   * faithful analog given the C caller already tried sweepapi. */
  if (!def || !def->run) {
    long long ttl = get_ttl_ms(db, id);
    int n = 0;
    char *fc = fc_from_intel(db, id, "miss", 0, ttl, &n);
    if (fc) return fc;
    if (!intel_has_source(db, id)) return NULL;   /* genuinely unknown */
    /* Known source id but zero geocoded rows → explicit empty FC. */
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "FeatureCollection");
    cJSON_AddArrayToObject(o, "features");
    cJSON *m = cJSON_AddObjectToObject(o, "_meta");
    cJSON_AddStringToObject(m, "source", id);
    char ts[40]; iso_now(ts, sizeof ts);
    cJSON_AddStringToObject(m, "fetchedAt", ts);
    cJSON_AddNumberToObject(m, "recordCount", 0);
    cJSON_AddBoolToObject(m, "live", 0);
    cJSON_AddStringToObject(m, "description",
                            "No collector registered for this source.");
    cJSON_AddStringToObject(m, "cache_status", "miss");
    cJSON_AddNumberToObject(m, "age_ms", 0);
    char *stt = source_status(db, id);
    cJSON_AddStringToObject(m, "status", stt ? stt : "unknown");
    free(stt);
    char *js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return js;
  }

  long long ttl = get_ttl_ms(db, id);

  /* ── 1. Cache HIT short-circuit ────────────────────────────────────────
   * data.js: when collector_cache is fresh, skip the run and serve the
   * intel_items reconstruction (cache_status:'hit'); fall back to the raw
   * cached FC for sources not yet mirrored. */
  {
    long long age = 0;
    char *cached_raw = cache_get(db, id, &age);
    if (cached_raw) {
      int n = 0;
      char *ifc = fc_from_intel(db, id, "hit", age, ttl, &n);
      if (ifc) {
        free(cached_raw);
        lw_finished(id, n, "hit");
        return ifc;
      }
      /* Fallback: stamp the cached FC's _meta like data.js does. */
      cJSON *fc = cJSON_Parse(cached_raw);
      free(cached_raw);
      if (fc) {
        cJSON *m = cJSON_GetObjectItem(fc, "_meta");
        if (!cJSON_IsObject(m)) m = cJSON_AddObjectToObject(fc, "_meta");
        cJSON_DeleteItemFromObject(m, "cache_status");
        cJSON_AddStringToObject(m, "cache_status", "hit");
        cJSON_DeleteItemFromObject(m, "age_ms");
        cJSON_AddNumberToObject(m, "age_ms", (double)age);
        cJSON_DeleteItemFromObject(m, "ttl_ms");
        cJSON_AddNumberToObject(m, "ttl_ms", (double)ttl);
        cJSON_DeleteItemFromObject(m, "served_from");
        cJSON_AddStringToObject(m, "served_from", "collector_cache");
        cJSON *fa = cJSON_GetObjectItem(fc, "features");
        int n2 = cJSON_IsArray(fa) ? cJSON_GetArraySize(fa) : 0;
        char *js = cJSON_PrintUnformatted(fc);
        cJSON_Delete(fc);
        lw_finished(id, n2, "hit");
        return js;
      }
      /* Unparseable cache row → treat as miss, fall through to run. */
    }
  }

  /* ── 2. Cache MISS — run the collector through the capture sink ─────────
   * data.js: broadcastLayerWorkStarted → collector() → mirror → respond.
   * Capture-sink run reconstructs Features exactly like lib/unified.c. */
  lw_started(id);

  cJSON *cap_arr = cJSON_CreateArray();
  cap_ctx cc = { cap_arr };
  intel_sink cap = { &cc, cap_emit };
  source_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.source_id = id;
  ctx.db = db;
  /* http/llm/entity/tenant left NULL: a scheduled-style on-demand run, same
   * as a non-pivot feed run. Sources needing HTTP/LLM that get NULL here
   * degrade exactly as they would on a misconfigured scheduled run (rc<0 →
   * empty, handled below) — no fabricated behaviour. */
  int rc = def->run(&ctx, &cap);

  /* allSettled semantics (lib/unified.c): a rejected run contributes nothing.
   * data.js's collector throw → 500; here the unified ABI returns rc<0 and
   * the faithful, more-graceful analog is the canonical empty FC (the same
   * shape data.js's no-data path produces) rather than surfacing a 500 from
   * a read endpoint. Documented choice. */
  cJSON *feats = cJSON_CreateArray();
  int emitted = 0;
  if (rc >= 0) {
    cJSON *f;
    cJSON_ArrayForEach(f, cap_arr) {
      cJSON *g = cJSON_GetObjectItem(f, "geometry");
      cJSON *p = cJSON_GetObjectItem(f, "properties");
      if (!g || !p) continue;                      /* mergeFC guard */
      cJSON_AddItemToArray(feats, cJSON_Duplicate(f, 1));
      emitted++;
    }
  }
  cJSON_Delete(cap_arr);

  /* ── Intel branch ──────────────────────────────────────────────────────
   * data.js: a non-spatial collector (data.kind === 'intel') yields an empty
   * FC tagged migrated_to_intel. In the unified ABI a non-geo source emits
   * items with no geometry/properties pair → zero captured Features. We
   * detect "ran OK, produced rows, but none geocoded" via intel_items having
   * rows for this source while the capture yielded nothing geocodable. */
  if (rc >= 0 && emitted == 0) {
    int has_rows = intel_has_source(db, id);
    if (has_rows) {
      /* Prefer the intel_items reconstruction (some rows may be geocoded by
       * a prior/concurrent real run); else the migrated-to-intel empty FC. */
      int n = 0;
      char *ifc = fc_from_intel(db, id, "miss", 0, ttl, &n);
      if (ifc) { cJSON_Delete(feats); lw_finished(id, n, "miss"); return ifc; }

      cJSON *fc = cJSON_CreateObject();
      cJSON_AddStringToObject(fc, "type", "FeatureCollection");
      cJSON_AddItemToObject(fc, "features", feats);   /* empty */
      cJSON *m = cJSON_AddObjectToObject(fc, "_meta");
      cJSON_AddStringToObject(m, "source", id);
      char ts[40]; iso_now(ts, sizeof ts);
      cJSON_AddStringToObject(m, "fetchedAt", ts);
      cJSON_AddNumberToObject(m, "recordCount", 0);
      cJSON_AddBoolToObject(m, "live", 0);
      cJSON_AddStringToObject(m, "description",
                              "Migrated to /api/intel/items");
      cJSON_AddBoolToObject(m, "migrated_to_intel", 1);
      char ep[320];
      snprintf(ep, sizeof ep, "/api/intel/items?source=%s", id);
      cJSON_AddStringToObject(m, "intel_endpoint", ep);
      cJSON_AddStringToObject(m, "cache_status", "miss");
      cJSON_AddNumberToObject(m, "age_ms", 0);
      char *js = cJSON_PrintUnformatted(fc);
      cJSON_Delete(fc);
      lw_finished(id, 0, "miss");
      return js;
    }
  }

  /* ── normaliseFc + cache the collector FC ──────────────────────────────
   * Assemble the canonical envelope from the captured Features, persist it
   * to collector_cache for the TTL window (data.js: setCached then prefer
   * the intel_items reconstruction, fall back to this FC). */
  {
    cJSON *fc = cJSON_CreateObject();
    cJSON_AddStringToObject(fc, "type", "FeatureCollection");
    cJSON_AddItemToObject(fc, "features", feats);    /* ownership moves */
    add_meta(fc, id, emitted, emitted > 0, NULL, NULL, -1, -1, NULL);
    char *raw = cJSON_PrintUnformatted(fc);
    if (raw) { cache_set(db, id, raw, ttl); free(raw); }

    /* Prefer intel_items reconstruction (carries record_type/sub_source_id/
     * geom_source the cached FC lacks); fall back to the normalised FC. */
    int n = 0;
    char *ifc = fc_from_intel(db, id, "miss", 0, ttl, &n);
    if (ifc) { cJSON_Delete(fc); lw_finished(id, n, "miss"); return ifc; }

    /* Fallback path: re-stamp _meta with cache fields + served_from. */
    cJSON *m = cJSON_GetObjectItem(fc, "_meta");
    cJSON_AddStringToObject(m, "cache_status", "miss");
    cJSON_DeleteItemFromObject(m, "age_ms");
    cJSON_AddNumberToObject(m, "age_ms", 0);
    cJSON_AddNumberToObject(m, "ttl_ms", (double)ttl);
    cJSON_AddStringToObject(m, "served_from", "collector_cache");
    char *js = cJSON_PrintUnformatted(fc);
    cJSON_Delete(fc);
    lw_finished(id, emitted, "miss");
    return js;
  }
}
