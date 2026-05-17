/* core/camera_store.c — faithful port of server/src/utils/cameraStore.js.
 * See camera_store.h. Master-only (intel_items); writes via the existing
 * intel_sink, reads via direct sqlite3 (== selectGeoFeatures / cameraStats).
 *
 * Ported surface (1:1 with the JS export set this subsystem needs):
 *   upsertCamera        -> camera_upsert
 *   upsertCamerasTx     -> loop of camera_upsert at the call site (the JS
 *                          `db.transaction` wrapper is just a perf envelope;
 *                          the camera runner / discovery source loops here)
 *   getAllCameras       -> camera_fc_json
 *   cameraStats         -> camera_stats_json
 *   getRecentCameras    -> camera_recent_json (kept internal-linkage-free for
 *                          the runner; same SELECT … ORDER BY fetched_at)
 * applyGeocodeOk/Fail, getCameraByUid, getDiscoveryFeed are LLM-enricher /
 * read-endpoint concerns out of scope for this camera-side sweep deliverable
 * (the read endpoints + enricher are separate plan items, §C/§D.3-12).
 */
#include "camera_store.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#define CAMERA_SOURCE_ID "camera-discovery"

/* Node `new Date().toISOString()` — YYYY-MM-DDTHH:MM:SS.mmmZ. */
static void iso_now(char *b, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(b, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}

/* prevProps.<k> as a non-empty STRING, else NULL (=== JS `||` truthiness for
 * the merged string fields: '' and null and absent all fall through). */
static const char *str_or_null(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
  if (v && cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  return NULL;
}

/* JS object-spread merge with last-write-wins value but first-seen position.
 * After `cJSON_AddItemToObject` the key exists; to overwrite while keeping
 * position we Replace; to add a new key we Append. This matches
 * `{ ...p, ...prevProps }` byte-for-byte (V8 keeps initial insertion order,
 * later assignment only changes the value). `src` items are duplicated. */
static void spread_into(cJSON *dst, cJSON *src) {
  if (!src || !cJSON_IsObject(src)) return;
  cJSON *it;
  cJSON_ArrayForEach(it, src) {
    cJSON *dup = cJSON_Duplicate(it, 1);
    if (cJSON_GetObjectItemCaseSensitive(dst, it->string))
      cJSON_ReplaceItemInObjectCaseSensitive(dst, it->string, dup);
    else
      cJSON_AddItemToObject(dst, it->string, dup);
  }
}

/* Set/replace a key while preserving position if it already exists (used for
 * the explicit named overrides — they are always already present because
 * makeFeature seeds camera_uid/name/camera_type/discovery_channel/country and
 * upsertCamera always writes the rest). */
static void set_key(cJSON *o, const char *k, cJSON *v) {
  if (cJSON_GetObjectItemCaseSensitive(o, k))
    cJSON_ReplaceItemInObjectCaseSensitive(o, k, v);
  else
    cJSON_AddItemToObject(o, k, v);
}
static void set_str_or_null(cJSON *o, const char *k, const char *v) {
  set_key(o, k, v ? cJSON_CreateString(v) : cJSON_CreateNull());
}

/* Set-membership over a JSON string array (insertion order preserved by the
 * callers that build the array). */
static int arr_has(cJSON *arr, const char *str) {
  cJSON *e;
  cJSON_ArrayForEach(e, arr)
    if (cJSON_IsString(e) && strcmp(e->valuestring, str) == 0) return 1;
  return 0;
}

/* Read the prior intel_items row's properties JSON (parsed) for uid, or NULL.
 * Mirrors getItemBySourceUid(CAMERA_SOURCE_ID, camera_uid).properties. */
static cJSON *prev_props(db_handle *db, const char *uid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT properties FROM intel_items WHERE uid=?1", -1, &s, NULL)
      != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  cJSON *p = NULL;
  if (sqlite3_step(s) == SQLITE_ROW &&
      sqlite3_column_type(s, 0) != SQLITE_NULL) {
    const char *txt = (const char *)sqlite3_column_text(s, 0);
    if (txt && *txt) p = cJSON_Parse(txt);
  }
  sqlite3_finalize(s);
  return p;
}

int camera_upsert(db_handle *db, intel_sink *sink, cJSON *feature,
                  const char *channel) {
  if (!feature) return -1;
  cJSON *p = cJSON_GetObjectItemCaseSensitive(feature, "properties");
  cJSON *geom = cJSON_GetObjectItemCaseSensitive(feature, "geometry");
  cJSON *coords = geom ? cJSON_GetObjectItemCaseSensitive(geom, "coordinates")
                       : NULL;
  cJSON *cu = p ? cJSON_GetObjectItemCaseSensitive(p, "camera_uid") : NULL;
  if (!cu || !cJSON_IsString(cu) || !cu->valuestring[0]) return -1;
  if (!coords || !cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2)
    return -1;
  cJSON *jlon = cJSON_GetArrayItem(coords, 0);
  cJSON *jlat = cJSON_GetArrayItem(coords, 1);
  if (!jlon || !jlat || !cJSON_IsNumber(jlon) || !cJSON_IsNumber(jlat))
    return -1;
  double lon = jlon->valuedouble, lat = jlat->valuedouble;
  if (!isfinite(lat) || !isfinite(lon)) return -1;

  const char *camera_uid = cu->valuestring;
  char master_uid[600];
  snprintf(master_uid, sizeof master_uid, "%s|%s",
           CAMERA_SOURCE_ID, camera_uid);

  cJSON *pp = prev_props(db, master_uid);          /* may be NULL */

  /* discovery_channels = Array.from(new Set([...prev, channel, ...p.dc]
   *                                          .filter(Boolean))) */
  cJSON *next_ch = cJSON_CreateArray();
  cJSON *prev_ch = pp ? cJSON_GetObjectItemCaseSensitive(pp,
                          "discovery_channels") : NULL;
  if (prev_ch && cJSON_IsArray(prev_ch)) {
    cJSON *e;
    cJSON_ArrayForEach(e, prev_ch)
      if (cJSON_IsString(e) && e->valuestring[0] &&
          !arr_has(next_ch, e->valuestring))
        cJSON_AddItemToArray(next_ch, cJSON_CreateString(e->valuestring));
  }
  if (channel && *channel && !arr_has(next_ch, channel))
    cJSON_AddItemToArray(next_ch, cJSON_CreateString(channel));
  cJSON *p_ch = p ? cJSON_GetObjectItemCaseSensitive(p,
                       "discovery_channels") : NULL;
  if (p_ch && cJSON_IsArray(p_ch)) {
    cJSON *e;
    cJSON_ArrayForEach(e, p_ch)
      if (cJSON_IsString(e) && e->valuestring[0] &&
          !arr_has(next_ch, e->valuestring))
        cJSON_AddItemToArray(next_ch, cJSON_CreateString(e->valuestring));
  }

  /* prevProps.X || p.X || default  (existing-non-null-wins) */
  const char *m_name  = str_or_null(pp, "name");
  if (!m_name) m_name = str_or_null(p, "name");
  if (!m_name) m_name = "Unknown camera";
  const char *m_type  = str_or_null(pp, "camera_type");
  if (!m_type) m_type = str_or_null(p, "camera_type");
  if (!m_type) m_type = "unknown";
  const char *m_url   = str_or_null(pp, "url");
  if (!m_url) m_url = str_or_null(p, "url");
  const char *m_thumb = str_or_null(pp, "thumbnail_url");
  if (!m_thumb) m_thumb = str_or_null(p, "thumbnail_url");
  const char *m_oper  = str_or_null(pp, "operator");
  if (!m_oper) m_oper = str_or_null(p, "operator");
  const char *m_ctry  = str_or_null(pp, "country");
  if (!m_ctry) m_ctry = str_or_null(p, "country");
  if (!m_ctry) m_ctry = "JP";

  char now_iso[40]; iso_now(now_iso, sizeof now_iso);
  const char *m_first = str_or_null(pp, "first_seen_at");
  char first_buf[40];
  if (!m_first) { snprintf(first_buf, sizeof first_buf, "%s", now_iso);
                  m_first = first_buf; }

  /* seen_count = (prevProps.seen_count ?? 0) + 1 */
  double seen = 0;
  cJSON *psc = pp ? cJSON_GetObjectItemCaseSensitive(pp, "seen_count") : NULL;
  if (psc && cJSON_IsNumber(psc)) seen = psc->valuedouble;
  seen += 1;

  /* mergedProps = { ...p, ...prevProps, <named overrides> } */
  cJSON *m = cJSON_CreateObject();
  spread_into(m, p);
  spread_into(m, pp);
  set_str_or_null(m, "camera_uid", camera_uid);
  set_str_or_null(m, "name", m_name);
  set_str_or_null(m, "camera_type", m_type);
  set_str_or_null(m, "url", m_url);
  set_str_or_null(m, "thumbnail_url", m_thumb);
  set_str_or_null(m, "operator", m_oper);
  set_str_or_null(m, "country", m_ctry);
  set_key(m, "discovery_channels", next_ch);       /* ownership → m */
  set_str_or_null(m, "first_seen_at", m_first);
  set_str_or_null(m, "last_seen_at", now_iso);
  set_key(m, "seen_count", cJSON_CreateNumber(seen));

  /* Build the master Point geometry exactly as the JS upsertItemSync arg
   * ({ type:'Point', coordinates:[lon,lat] }). */
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *gc = cJSON_CreateArray();
  cJSON_AddItemToArray(gc, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(gc, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", gc);

  char *props_json = cJSON_PrintUnformatted(m);
  char *geom_json  = cJSON_PrintUnformatted(g);

  /* geomSource: the JS picks 'llm' iff the existing master row was llm-geo;
   * the intel_sink already preserves geom_source='llm' via its ON CONFLICT
   * CASE, so emitting has_geo=1 (geom_source='native') yields the identical
   * stored row. record_type='camera', title=mergedName. */
  intel_item it = {0};
  it.uid = master_uid;
  it.title = m_name;
  it.has_geo = 1;
  it.lat = lat;
  it.lon = lon;
  it.geometry_geojson = geom_json;
  it.record_type = "camera";
  it.properties_json = props_json ? props_json : "{}";

  int rc = sink->emit(sink, &it);

  free(props_json);
  free(geom_json);
  cJSON_Delete(g);
  cJSON_Delete(m);
  if (pp) cJSON_Delete(pp);
  return rc;            /* 1 new / 0 updated / <0 error — same as JS kind */
}

/* selectGeoFeatures({sourceId}) row→Feature shaping: parsed props spread,
 * then the fixed overlay keys in EXACT JS order, geometry from the geometry
 * column else a Point fallback from lat/lon. */
static cJSON *row_to_feature(sqlite3_stmt *s) {
  /* cols: 0 uid,1 source_id,2 sub_source_id,3 record_type,4 lat,5 lon,
   *       6 geometry,7 title,8 summary,9 link,10 language,
   *       11 published_at,12 fetched_at,13 properties,14 tags */
  const char *c_uid  = (const char *)sqlite3_column_text(s, 0);
  const char *c_src  = (const char *)sqlite3_column_text(s, 1);
  int subnull = sqlite3_column_type(s, 2) == SQLITE_NULL;
  const char *c_sub  = subnull ? NULL : (const char *)sqlite3_column_text(s,2);
  int rtnull = sqlite3_column_type(s, 3) == SQLITE_NULL;
  const char *c_rt   = rtnull ? NULL : (const char *)sqlite3_column_text(s,3);
  int latnull = sqlite3_column_type(s, 4) == SQLITE_NULL;
  int lonnull = sqlite3_column_type(s, 5) == SQLITE_NULL;
  double lat = sqlite3_column_double(s, 4), lon = sqlite3_column_double(s, 5);
  int gnull = sqlite3_column_type(s, 6) == SQLITE_NULL;
  const char *c_geom = gnull ? NULL : (const char *)sqlite3_column_text(s, 6);
  int tnull = sqlite3_column_type(s, 7) == SQLITE_NULL;
  const char *c_title = tnull ? NULL : (const char *)sqlite3_column_text(s,7);
  int sumnull = sqlite3_column_type(s, 8) == SQLITE_NULL;
  const char *c_summ = sumnull ? NULL : (const char *)sqlite3_column_text(s,8);
  int lknull = sqlite3_column_type(s, 9) == SQLITE_NULL;
  const char *c_link = lknull ? NULL : (const char *)sqlite3_column_text(s,9);
  int lgnull = sqlite3_column_type(s, 10) == SQLITE_NULL;
  const char *c_lang = lgnull ? NULL : (const char *)sqlite3_column_text(s,10);
  int pbnull = sqlite3_column_type(s, 11) == SQLITE_NULL;
  const char *c_pub  = pbnull ? NULL : (const char *)sqlite3_column_text(s,11);
  int ftnull = sqlite3_column_type(s, 12) == SQLITE_NULL;
  const char *c_fetch = ftnull ? NULL : (const char *)sqlite3_column_text(s,12);
  int prnull = sqlite3_column_type(s, 13) == SQLITE_NULL;
  const char *c_props = prnull ? NULL : (const char *)sqlite3_column_text(s,13);
  int tgnull = sqlite3_column_type(s, 14) == SQLITE_NULL;
  const char *c_tags = tgnull ? NULL : (const char *)sqlite3_column_text(s,14);

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

  /* properties: { ...properties, uid, source_id, sub_source_id, record_type,
   *               title, summary, link, language, published_at,
   *               fetched_at, tags } — overlay keys overwrite spread ones in
   *               place, exactly like the JS object literal. */
  cJSON *pr = cJSON_CreateObject();
  spread_into(pr, props);
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
  set_key(pr, "tags", cJSON_Duplicate(tags, 1));
  cJSON_AddItemToObject(out, "properties", pr);

  cJSON_Delete(props);
  cJSON_Delete(tags);
  return out;
}

char *camera_fc_json(db_handle *db) {
  /* selectGeoFeatures default: where source_id=? AND lat IS NOT NULL. */
  static const char *Q =
    "SELECT uid, source_id, sub_source_id, record_type, lat, lon, geometry,"
    "       title, summary, link, language, published_at, fetched_at,"
    "       properties, tags"
    "  FROM intel_items"
    " WHERE source_id = ?1 AND lat IS NOT NULL";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, Q, -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, CAMERA_SOURCE_ID, -1, SQLITE_TRANSIENT);

  cJSON *features = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW)
    cJSON_AddItemToArray(features, row_to_feature(s));
  sqlite3_finalize(s);

  int count = cJSON_GetArraySize(features);
  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON_AddItemToObject(fc, "features", features);
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();        /* getAllCameras _meta block */
  cJSON_AddStringToObject(meta, "source", "camera_store");
  cJSON_AddNumberToObject(meta, "recordCount", count);
  cJSON_AddStringToObject(meta, "fetchedAt", ts);
  cJSON_AddStringToObject(meta, "served_from", "intel_items");
  cJSON_AddItemToObject(fc, "_meta", meta);

  char *js = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return js;
}

char *camera_stats_json(db_handle *db) {
  /* total + new24h (fetched_at >= datetime('now','-24 hours')). */
  long total = 0, new24h = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT COUNT(*),"
        " SUM(CASE WHEN fetched_at >= datetime('now','-24 hours')"
        "          THEN 1 ELSE 0 END)"
        " FROM intel_items WHERE source_id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, CAMERA_SOURCE_ID, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      total  = (long)sqlite3_column_int64(s, 0);
      new24h = (long)sqlite3_column_int64(s, 1);
    }
    sqlite3_finalize(s);
  }

  cJSON *by_type = cJSON_CreateArray();
  if (sqlite3_prepare_v2(db->h,
        "SELECT json_extract(properties,'$.camera_type') AS camera_type,"
        "       COUNT(*) AS c"
        "  FROM intel_items WHERE source_id=?1"
        " GROUP BY json_extract(properties,'$.camera_type')",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, CAMERA_SOURCE_ID, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(s) == SQLITE_ROW) {
      cJSON *r = cJSON_CreateObject();
      if (sqlite3_column_type(s, 0) == SQLITE_NULL)
        cJSON_AddNullToObject(r, "camera_type");
      else
        cJSON_AddStringToObject(r, "camera_type",
          (const char *)sqlite3_column_text(s, 0));
      cJSON_AddNumberToObject(r, "c", (double)sqlite3_column_int64(s, 1));
      cJSON_AddItemToArray(by_type, r);
    }
    sqlite3_finalize(s);
  }

  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "total", (double)total);
  cJSON_AddNumberToObject(o, "new24h", (double)new24h);
  cJSON_AddItemToObject(o, "byType", by_type);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── getDiscoveryFeed ─────────────────────────────────────────────────────
 * Standard base64 (RFC4648 +/ , '=' pad) — round-trips Node's
 * Buffer.from(s).toString('base64') / Buffer.from(c,'base64'). */
static char *b64_enc(const char *in, size_t len) {
  static const char T[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *o = malloc(((len + 2) / 3) * 4 + 1);
  if (!o) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = (unsigned char)in[i];
    unsigned b = i + 1 < len ? (unsigned char)in[i + 1] : 0;
    unsigned cc = i + 2 < len ? (unsigned char)in[i + 2] : 0;
    unsigned v = (a << 16) | (b << 8) | cc;
    o[j++] = T[(v >> 18) & 63];
    o[j++] = T[(v >> 12) & 63];
    o[j++] = i + 1 < len ? T[(v >> 6) & 63] : '=';
    o[j++] = i + 2 < len ? T[v & 63] : '=';
  }
  o[j] = 0;
  return o;
}
static int b64_val(int ch) {
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9') return ch - '0' + 52;
  if (ch == '+') return 62;
  if (ch == '/') return 63;
  return -1;
}
static char *b64_dec(const char *in) {            /* NUL-terminated → str */
  size_t len = strlen(in);
  char *o = malloc(len / 4 * 3 + 4);
  if (!o) return NULL;
  size_t j = 0; unsigned v = 0; int bits = 0;
  for (size_t i = 0; i < len; i++) {
    if (in[i] == '=') break;
    int d = b64_val((unsigned char)in[i]);
    if (d < 0) continue;
    v = (v << 6) | (unsigned)d; bits += 6;
    if (bits >= 8) { bits -= 8; o[j++] = (char)((v >> bits) & 0xFF); }
  }
  o[j] = 0;
  return o;
}

char *camera_discovery_feed(db_handle *db, int limit,
                            const char *cursor, const char *channel) {
  int cap = limit > 0 ? limit : 500;
  if (cap < 1) cap = 1;
  if (cap > 5000) cap = 5000;

  /* cursor: base64 "ts|uid" → strictly-older keyset (malformed ⇒ ignored) */
  char *cur_dec = NULL, *cur_ts = NULL, *cur_uid = NULL;
  if (cursor && *cursor) {
    cur_dec = b64_dec(cursor);
    if (cur_dec) {
      char *bar = strchr(cur_dec, '|');
      if (bar && bar > cur_dec) { *bar = 0; cur_ts = cur_dec; cur_uid = bar + 1; }
    }
  }

  /* WHERE (== JS: source + record_type + lat NOT NULL + channel? + cursor?).
   * Reused ?N indices bind once; indices stay contiguous from ?1. */
  char w[1200]; size_t wl = 0;
  wl += snprintf(w + wl, sizeof w - wl,
    "source_id=?1 AND record_type IN ('camera','camera-discovery') "
    "AND lat IS NOT NULL");
  int bi = 2, ch_i = 0, cts_i = 0, cuid_i = 0;
  if (channel && *channel) {
    ch_i = bi++;
    wl += snprintf(w + wl, sizeof w - wl,
      " AND (json_extract(properties,'$.discovery_channel')=?%d "
      "OR EXISTS (SELECT 1 FROM json_each(json_extract(properties,"
      "'$.discovery_channels')) WHERE json_each.value=?%d))", ch_i, ch_i);
  }
  if (cur_ts && cur_uid) {
    cts_i = bi++; cuid_i = bi++;
    wl += snprintf(w + wl, sizeof w - wl,
      " AND (COALESCE(json_extract(properties,'$.last_seen_at'),fetched_at)<?%d "
      "OR (COALESCE(json_extract(properties,'$.last_seen_at'),fetched_at)=?%d "
      "AND uid<?%d))", cts_i, cts_i, cuid_i);
  }
  int lim_i = bi;

  char sql[1500];
  snprintf(sql, sizeof sql,
    "SELECT uid,lat,lon,geometry,properties,fetched_at,"
    "COALESCE(json_extract(properties,'$.last_seen_at'),fetched_at) AS ts "
    "FROM intel_items WHERE %s ORDER BY ts DESC, uid DESC LIMIT ?%d",
    w, lim_i);

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK) {
    free(cur_dec); return NULL;
  }
  sqlite3_bind_text(s, 1, CAMERA_SOURCE_ID, -1, SQLITE_STATIC);
  if (ch_i)  sqlite3_bind_text(s, ch_i, channel, -1, SQLITE_TRANSIENT);
  if (cts_i) {
    sqlite3_bind_text(s, cts_i,  cur_ts,  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, cuid_i, cur_uid, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(s, lim_i, cap + 1);

  cJSON *events = cJSON_CreateArray();
  int n = 0, has_more = 0;
  char last_ts[64] = {0}, last_uid[600] = {0};
  while (sqlite3_step(s) == SQLITE_ROW) {
    if (n >= cap) { has_more = 1; break; }      /* the (cap+1)th row exists */
    const char *uid = (const char *)sqlite3_column_text(s, 0);
    int lat_null = sqlite3_column_type(s, 1) == SQLITE_NULL;
    int lon_null = sqlite3_column_type(s, 2) == SQLITE_NULL;
    double lat = sqlite3_column_double(s, 1);
    double lon = sqlite3_column_double(s, 2);
    const char *gj = (const char *)sqlite3_column_text(s, 3);
    const char *pj = (const char *)sqlite3_column_text(s, 4);
    const char *ts = (const char *)sqlite3_column_text(s, 6);

    cJSON *props = pj ? cJSON_Parse(pj) : NULL;
    if (!props) props = cJSON_CreateObject();
    cJSON *geom = gj ? cJSON_Parse(gj) : NULL;
    if (!geom && !lat_null && !lon_null) {
      geom = cJSON_CreateObject();
      cJSON_AddStringToObject(geom, "type", "Point");
      cJSON *coords = cJSON_CreateArray();
      cJSON_AddItemToArray(coords, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(coords, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(geom, "coordinates", coords);
    }

    /* channel = props.discovery_channel || discovery_channels[0] || 'unknown' */
    const char *ev_ch = NULL;
    cJSON *dc = cJSON_GetObjectItemCaseSensitive(props, "discovery_channel");
    if (dc && cJSON_IsString(dc) && dc->valuestring[0]) ev_ch = dc->valuestring;
    if (!ev_ch) {
      cJSON *dcs = cJSON_GetObjectItemCaseSensitive(props, "discovery_channels");
      if (dcs && cJSON_IsArray(dcs)) {
        cJSON *f0 = cJSON_GetArrayItem(dcs, 0);
        if (f0 && cJSON_IsString(f0) && f0->valuestring[0]) ev_ch = f0->valuestring;
      }
    }
    if (!ev_ch) ev_ch = "unknown";

    /* camera.properties = { ...props, camera_uid: props.camera_uid ?? uid-after-'|' } */
    cJSON *cprops = cJSON_Duplicate(props, 1);
    cJSON *cu = cJSON_GetObjectItemCaseSensitive(cprops, "camera_uid");
    if (!cu || cJSON_IsNull(cu)) {
      const char *bar = uid ? strchr(uid, '|') : NULL;
      const char *fb = bar ? bar + 1 : (uid ? uid : "");
      if (cu) cJSON_ReplaceItemInObjectCaseSensitive(cprops, "camera_uid",
                                                     cJSON_CreateString(fb));
      else    cJSON_AddStringToObject(cprops, "camera_uid", fb);
    }
    cJSON *cam = cJSON_CreateObject();
    cJSON_AddStringToObject(cam, "type", "Feature");
    cJSON_AddItemToObject(cam, "geometry", geom ? geom : cJSON_CreateNull());
    cJSON_AddItemToObject(cam, "properties", cprops);

    cJSON *ev = cJSON_CreateObject();
    cJSON_AddStringToObject(ev, "ts", ts ? ts : "");
    cJSON_AddStringToObject(ev, "kind", "historical");
    cJSON_AddStringToObject(ev, "channel", ev_ch);
    cJSON_AddItemToObject(ev, "camera", cam);
    cJSON_AddItemToObject(ev, "run_id", cJSON_CreateNull());
    cJSON_AddItemToArray(events, ev);

    snprintf(last_ts,  sizeof last_ts,  "%s", ts  ? ts  : "");
    snprintf(last_uid, sizeof last_uid, "%s", uid ? uid : "");
    cJSON_Delete(props);
    n++;
  }
  sqlite3_finalize(s);
  free(cur_dec);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "events", events);
  if (has_more && last_uid[0]) {
    char raw[700];
    snprintf(raw, sizeof raw, "%s|%s", last_ts, last_uid);
    char *enc = b64_enc(raw, strlen(raw));
    if (enc) { cJSON_AddStringToObject(root, "cursor", enc); free(enc); }
    else       cJSON_AddItemToObject(root, "cursor", cJSON_CreateNull());
  } else {
    cJSON_AddItemToObject(root, "cursor", cJSON_CreateNull());
  }
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}
