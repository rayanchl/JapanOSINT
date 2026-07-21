#include "miscapi.h"
#include "source_registry.h"
#include "statusapi.h"          /* statusapi_strip_has (shared STRIP set) */
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* one SELECT * row → cJSON object, sqlite type → JSON type (mirrors
 * better-sqlite3 row objects + JSON.stringify). */
static cJSON *row_obj(sqlite3_stmt *s) {
  cJSON *o = cJSON_CreateObject();
  int n = sqlite3_column_count(s);
  for (int i = 0; i < n; i++) {
    const char *k = sqlite3_column_name(s, i);
    switch (sqlite3_column_type(s, i)) {
      case SQLITE_NULL:    cJSON_AddNullToObject(o, k); break;
      case SQLITE_INTEGER: cJSON_AddNumberToObject(o, k,
                              (double)sqlite3_column_int64(s, i)); break;
      case SQLITE_FLOAT:   cJSON_AddNumberToObject(o, k,
                              sqlite3_column_double(s, i)); break;
      default:             cJSON_AddStringToObject(o, k,
                              (const char *)sqlite3_column_text(s, i)); break;
    }
  }
  return o;
}

char *miscapi_source_by_id(db_handle *db, const char *id) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, "SELECT * FROM sources WHERE id = ?1",
                         -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  char *js = NULL;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *o = row_obj(s);
    js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
  }
  sqlite3_finalize(s);
  return js;                                    /* NULL → 404 */
}

char *miscapi_set_schedule(db_handle *db, const char *id, const char *body) {
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;
  cJSON *jm = jb ? cJSON_GetObjectItem(jb, "mode") : NULL;
  const char *mode = (jm && cJSON_IsString(jm)) ? jm->valuestring : "";
  int ok = !strcmp(mode, "map_cron") || !strcmp(mode, "search_only");
  if (!ok) { if (jb) cJSON_Delete(jb); return strdup("\1bad"); }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "UPDATE sources SET schedule_mode=?1 WHERE id=?2", -1, &s, NULL)
      != SQLITE_OK) { if (jb) cJSON_Delete(jb); return NULL; }
  sqlite3_bind_text(s, 1, mode, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, id, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  int changed = sqlite3_changes(db->h);
  sqlite3_finalize(s);
  if (jb) cJSON_Delete(jb);
  if (changed == 0) return NULL;                 /* unknown id → 404 */
  return miscapi_source_by_id(db, id);           /* updated row */
}

char *miscapi_source_logs(db_handle *db, const char *id, int limit) {
  /* getSourceById guard first — unknown id is a 404, not an empty list. */
  sqlite3_stmt *chk;
  int exists = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT 1 FROM sources WHERE id = ?1",
                         -1, &chk, NULL) == SQLITE_OK) {
    sqlite3_bind_text(chk, 1, id, -1, SQLITE_TRANSIENT);
    exists = sqlite3_step(chk) == SQLITE_ROW;
  }
  sqlite3_finalize(chk);
  if (!exists) return NULL;

  int lim = limit > 0 ? limit : 50;
  if (lim > 500) lim = 500;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT * FROM fetch_log WHERE source_id = ?1 "
        "ORDER BY timestamp DESC, id DESC LIMIT ?2", -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW)
    cJSON_AddItemToArray(arr, row_obj(s));
  sqlite3_finalize(s);
  char *js = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return js;
}

/* ── GET /api/layers (port of routes/layers.js getLayerDefinitions) ──────── */

/* layer.replace(/-/g,' ').replace(/\b\w/g, c=>c.toUpperCase()): kebab → Title
 * Case. A word char (\w = [A-Za-z0-9_]) is upper-cased iff it opens a word
 * (preceded by a non-word boundary); '-' becomes a space first. */
static void titlecase_kebab(const char *in, char *out, size_t cap) {
  size_t j = 0; int prev_word = 0;
  for (size_t i = 0; in[i] && j + 1 < cap; i++) {
    char c = in[i];
    if (c == '-') c = ' ';
    int is_word = c == '_' || (c >= '0' && c <= '9') ||
                  (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    if (is_word && !prev_word && c >= 'a' && c <= 'z') c = (char)(c - 32);
    out[j++] = c;
    prev_word = is_word;
  }
  out[j] = 0;
}

/* describeTemporal(layerId): liveOnly → {liveOnly:true}; static → nothing;
 * else → {temporal:{field:"published_at",fallbackField:"fetched_at"}}. */
static const char *TEMPORAL_LIVE_ONLY[] = { "unified-flights" };
static const char *TEMPORAL_STATIC[] = {
  "unified-trains", "unified-subways", "unified-buses",
  "unified-ais-ships", "unified-port-infra", "unified-stations",
  "unified-airports", "unified-station-footprints",
};
static int in_list(const char **list, size_t n, const char *id) {
  for (size_t i = 0; i < n; i++) if (strcmp(list[i], id) == 0) return 1;
  return 0;
}
static void apply_temporal(cJSON *layer, const char *id) {
  if (in_list(TEMPORAL_LIVE_ONLY, sizeof TEMPORAL_LIVE_ONLY / sizeof *TEMPORAL_LIVE_ONLY, id)) {
    cJSON_AddBoolToObject(layer, "liveOnly", 1);
    return;
  }
  if (in_list(TEMPORAL_STATIC, sizeof TEMPORAL_STATIC / sizeof *TEMPORAL_STATIC, id))
    return;
  cJSON *t = cJSON_CreateObject();
  cJSON_AddStringToObject(t, "field", "published_at");
  cJSON_AddStringToObject(t, "fallbackField", "fetched_at");
  cJSON_AddItemToObject(layer, "temporal", t);
}

/* UNIFIED_LAYER_PROVIDERS: underlying providers folded into each unified
 * layer's source list (dedup by id) so layer.sources.count reflects the real
 * provider count. NULL-terminated provider lists. */
static const struct { const char *layer; const char *providers[5]; } UNIFIED[] = {
  { "unified-trains",     { "mlit-n02-stations", "osm-transport-trains", NULL } },
  { "unified-subways",    { "mlit-n02-stations", "osm-transport-subways", NULL } },
  { "unified-buses",      { "mlit-p11-bus-stops", "gtfs-jp", "bus-routes", "osm-transport-buses", NULL } },
  { "unified-flights",    { "flight-adsb", NULL } },
  { "unified-airports",   { "mlit-p02-airports", NULL } },
  { "unified-highway",    { "highway-traffic", "jartic-traffic", NULL } },
  { "unified-port-infra", { "mlit-c02-ports", "osm-transport-ports", NULL } },
  { "unified-ais-ships",  { "maritime-ais", "marine-traffic", "vessel-finder", NULL } },
};

static cJSON *find_layer(cJSON *arr, const char *id) {
  cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    cJSON *iid = cJSON_GetObjectItem(it, "id");
    if (iid && cJSON_IsString(iid) && strcmp(iid->valuestring, id) == 0) return it;
  }
  return NULL;
}
static int sources_has(cJSON *sources, const char *id) {
  cJSON *it;
  cJSON_ArrayForEach(it, sources) {
    cJSON *sid = cJSON_GetObjectItem(it, "id");
    if (sid && cJSON_IsString(sid) && strcmp(sid->valuestring, id) == 0) return 1;
  }
  return 0;
}
static void add_source_ref(cJSON *sources, const src_meta *m) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", m->id);
  cJSON_AddItemToObject(o, "name", m->name ? cJSON_CreateString(m->name) : cJSON_CreateNull());
  cJSON_AddItemToObject(o, "type", m->type ? cJSON_CreateString(m->type) : cJSON_CreateNull());
  cJSON_AddBoolToObject(o, "free", m->free);
  cJSON_AddItemToArray(sources, o);
}

char *miscapi_list_layers(void) {
  cJSON *layers = cJSON_CreateArray();

  /* Group registry sources by their declared layer (registry order, first
   * seen wins for name/category). Sources with no layer are skipped — this is
   * the null-guard Node lacked (src.layer.replace on osint-search → 500). */
  int n = src_meta_count();
  for (int i = 0; i < n; i++) {
    const src_meta *m = src_meta_at(i);
    if (!m->layer || !m->layer[0]) continue;
    cJSON *layer = find_layer(layers, m->layer);
    if (!layer) {
      layer = cJSON_CreateObject();
      cJSON_AddStringToObject(layer, "id", m->layer);
      char name[256]; titlecase_kebab(m->layer, name, sizeof name);
      cJSON_AddStringToObject(layer, "name", name);
      cJSON_AddItemToObject(layer, "category",
        m->category ? cJSON_CreateString(m->category) : cJSON_CreateNull());
      cJSON_AddItemToObject(layer, "sources", cJSON_CreateArray());
      cJSON_AddItemToArray(layers, layer);
    }
    add_source_ref(cJSON_GetObjectItem(layer, "sources"), m);
  }

  /* Fold unified-* provider members into their parent layer (dedup by id). */
  for (size_t u = 0; u < sizeof UNIFIED / sizeof *UNIFIED; u++) {
    cJSON *bucket = find_layer(layers, UNIFIED[u].layer);
    if (!bucket) continue;
    cJSON *sources = cJSON_GetObjectItem(bucket, "sources");
    for (int p = 0; UNIFIED[u].providers[p]; p++) {
      const char *pid = UNIFIED[u].providers[p];
      if (sources_has(sources, pid)) continue;
      const src_meta *src = src_meta_get(pid);
      if (src) add_source_ref(sources, src);
    }
  }

  /* Drop STRIP_LAYER_IDS, attach temporal disposition, emit. */
  cJSON *out = cJSON_CreateArray();
  cJSON *layer;
  cJSON_ArrayForEach(layer, layers) {
    const char *id = cJSON_GetObjectItem(layer, "id")->valuestring;
    if (statusapi_strip_has(id)) continue;
    cJSON *dup = cJSON_Duplicate(layer, 1);
    apply_temporal(dup, id);
    cJSON_AddItemToArray(out, dup);
  }
  cJSON_Delete(layers);

  char *js = cJSON_PrintUnformatted(out);
  cJSON_Delete(out);
  return js;
}

char *miscapi_layer_geojson(const char *layer_id) {
  /* getLayerDefinitions(): a layer exists iff some registry source declares
   * it. data_cache is gone, so the body is the documented empty FC whose
   * _meta.sources lists that layer's contributing source ids. */
  cJSON *srcs = cJSON_CreateArray();
  int n = src_meta_count();
  for (int i = 0; i < n; i++) {
    const src_meta *m = src_meta_at(i);
    if (m->layer && strcmp(m->layer, layer_id) == 0)
      cJSON_AddItemToArray(srcs, cJSON_CreateString(m->id));
  }
  if (cJSON_GetArraySize(srcs) == 0) { cJSON_Delete(srcs); return NULL; }

  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "layer", layer_id);
  cJSON_AddStringToObject(meta, "message",
                          "Use /api/data/:layerId for live collector output.");
  cJSON_AddItemToObject(meta, "sources", srcs);

  cJSON *fc = cJSON_CreateObject();
  cJSON_AddStringToObject(fc, "type", "FeatureCollection");
  cJSON_AddItemToObject(fc, "features", cJSON_CreateArray());
  cJSON_AddItemToObject(fc, "_meta", meta);
  char *js = cJSON_PrintUnformatted(fc);
  cJSON_Delete(fc);
  return js;
}

char *miscapi_follow_recent(int limit) {
  (void)limit;
  /* No cross-process collector-tap ring buffer exists in the C server, so
   * there is no backing store to read. Return an honest 501 (caller sets the
   * status) rather than a fake-empty 200 clients can't distinguish. */
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", "not_implemented");
  cJSON_AddStringToObject(o, "detail",
    "collector-tap history is not supported by the native server");
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}
