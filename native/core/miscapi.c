#include "miscapi.h"
#include "source_registry.h"
#include "layertab.h"           /* curated taxonomy + source→layer resolver */
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
  /* sqlite3_changes() reports the last COMPLETED write on the connection, so
   * reading it after an unchecked step meant a failed UPDATE (SQLITE_BUSY
   * behind the 5 s timeout while a collector holds the write lock) inherited
   * some earlier statement's count — non-zero — and the endpoint answered 200
   * with the row still carrying the OLD schedule_mode. */
  int done = sqlite3_step(s) == SQLITE_DONE;
  int changed = done ? sqlite3_changes(db->h) : -1;
  sqlite3_finalize(s);
  if (jb) cJSON_Delete(jb);
  if (changed < 0) return strdup("\1err");       /* write failed → 500 */
  if (changed == 0) return NULL;                 /* unknown id → 404 */
  return miscapi_source_by_id(db, id);           /* updated row */
}

/* Node's new Date().toISOString(), the modulo-clamped spelling used across the
 * tree so -Wformat-truncation can prove the 24 chars fit. */
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
           (unsigned)(tm.tm_year + 1900) % 10000u, (unsigned)(tm.tm_mon + 1) % 100u,
           (unsigned)tm.tm_mday % 100u, (unsigned)tm.tm_hour % 100u,
           (unsigned)tm.tm_min % 100u, (unsigned)tm.tm_sec % 100u,
           (unsigned)(tv.tv_usec / 1000) % 1000u);
}

/* GET /api/sources/:id/logs
 *
 * DELIBERATE RESPONSE-SHAPE CHANGE — this route used to answer a BARE JSON
 * ARRAY, `[{…},{…}]`, capped at 500 rows with no total and no paging. With 800
 * fetch_log rows for a source it returned 500 of them and the body contained
 * nothing that could distinguish that from all 500 there were. House rule 2
 * requires the bounded view to state in-band how much it is showing out of how
 * much exists, and a bare array has nowhere in-band to state it: there is no
 * key to add. Keeping the array meant keeping the violation.
 *
 * So the array moved under "data" and gained the same page/meta block every
 * other list in this API now carries. This BREAKS a client doing
 * `logs.map(...)` on the response; that was weighed, not overlooked:
 *
 *   - No caller in this repository consumes the route (grepped across the
 *     Swift/TS/JS/HTML sources — the only hits are httpd.c's own wiring and
 *     miscapi.h). It is a diagnostics endpoint for the operator console.
 *   - No contract fixture covers it. tests/contract/run.sh gates exactly five
 *     routes (/api/status, /api/sources, /api/layers, /api/intel/sources,
 *     /api/intel/items?limit=10) and this is not one of them, so no
 *     irreplaceable *.node.json baseline describes its shape.
 *   - The tree has already made this same call once: entityapi_search's
 *     no-usable-token path used to print a bare `[]` and was changed to
 *     {"results":[]} precisely because a client reading a keyed field off a
 *     bare array gets undefined.
 *
 * `offset` is new for the same reason the envelope is: rows past the 500 cap
 * were previously unreachable, and disclosing a total the caller cannot then
 * page to is only half of rule 2. ORDER BY is unchanged and was already a
 * total order (timestamp DESC, id DESC — id is the INTEGER PRIMARY KEY), so
 * paging across a boundary cannot repeat or drop a row. */
char *miscapi_source_logs(db_handle *db, const char *id, int limit, int offset) {
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
  int off = offset > 0 ? offset : 0;

  /* Measured, over the identical predicate. -1 (rendered as null) if the count
   * could not be taken: rule 1 — never report a number we did not obtain. */
  long total = -1;
  { sqlite3_stmt *cs = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT COUNT(*) FROM fetch_log WHERE source_id = ?1",
          -1, &cs, NULL) == SQLITE_OK) {
      sqlite3_bind_text(cs, 1, id, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(cs) == SQLITE_ROW) total = (long)sqlite3_column_int64(cs, 0);
    }
    sqlite3_finalize(cs); }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT * FROM fetch_log WHERE source_id = ?1 "
        "ORDER BY timestamp DESC, id DESC LIMIT ?2 OFFSET ?3",
        -1, &s, NULL) != SQLITE_OK)
    return NULL;
  sqlite3_bind_text(s, 1, id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, lim);
  sqlite3_bind_int(s, 3, off);
  cJSON *arr = cJSON_CreateArray();
  while (sqlite3_step(s) == SQLITE_ROW)
    cJSON_AddItemToArray(arr, row_obj(s));
  sqlite3_finalize(s);

  cJSON *page = cJSON_CreateObject();
  cJSON_AddNumberToObject(page, "limit", lim);
  cJSON_AddNumberToObject(page, "offset", off);
  if (total < 0) cJSON_AddNullToObject(page, "total");
  else           cJSON_AddNumberToObject(page, "total", (double)total);

  cJSON *filters = cJSON_CreateObject();
  cJSON_AddStringToObject(filters, "source_id", id);
  char ts[40]; iso_now(ts, sizeof ts);
  cJSON *meta = cJSON_CreateObject();
  cJSON_AddStringToObject(meta, "fetched_at", ts);
  cJSON_AddItemToObject(meta, "filters", filters);

  cJSON *o = cJSON_CreateObject();
  cJSON_AddItemToObject(o, "data", arr);
  cJSON_AddItemToObject(o, "page", page);
  cJSON_AddItemToObject(o, "meta", meta);
  char *js = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return js;
}

/* ── GET /api/layers — the layer TAXONOMY (v2) ───────────────────────────
 *
 * v1 was a port of routes/layers.js: a layer existed iff some source declared
 * a `.layer` string, and statusapi's STRIP[] then hid every multi-source
 * thematic id, so the ~13,000 generated sources (whose `.layer` is NULL by
 * construction) and every geocoded row behind them were unreachable from the
 * map. v2 replaces that accident with the curated table in core/layers.def:
 * layers group by DATA TYPE and MODALITY (declared, never inferred — see the
 * table header for the points-vs-heatmap rule), every registered source
 * resolves to at most ONE layer (core/layertab.c), and any geocoded
 * intel_items row whose source resolves to NO layer is surfaced through a
 * GENERATED per-record_type layer (`rt-<slug>` / `unassigned-geocoded`)
 * derived from the database itself. The three kinds are labelled in-band
 * ("curated" | "declared" | "generated") and each layer carries a measured
 * `records_geocoded`, so the layer list is also the coverage proof: summing
 * records_geocoded over all layers equals COUNT(*) WHERE lat IS NOT NULL —
 * exactly, because assignment is a partition.
 *
 * The response stays a BARE ARRAY (v1 shape) so existing clients keep
 * decoding; every element gains data_type / modality / kind /
 * records_geocoded. The captured contract lives in
 * tests/contract/_api_layers_v2.json. */

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

/* (The old UNIFIED_LAYER_PROVIDERS fold lives on as explicit id:/layer: match
 * terms on the unified-* rows in core/layers.def — same members, but now under
 * the single-assignment rule, so a provider can no longer be listed under two
 * layers at once. mlit-n02-stations, which Node folded into BOTH trains and
 * subways, resolves to unified-trains alone; that is the price of per-layer
 * record counts that sum exactly to the geocoded total.) */

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

/* One layer skeleton with the v2 keys in a fixed order. data_type / modality
 * may be NULL → JSON null: "no one has declared this yet" is a different
 * claim from any concrete value, and clients must be able to tell (rule 1). */
static cJSON *make_layer(const char *id, const char *name, const char *category,
                         const char *data_type, const char *modality,
                         const char *kind) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", id);
  cJSON_AddStringToObject(o, "name", name);
  cJSON_AddItemToObject(o, "category",
    category ? cJSON_CreateString(category) : cJSON_CreateNull());
  cJSON_AddItemToObject(o, "data_type",
    data_type ? cJSON_CreateString(data_type) : cJSON_CreateNull());
  cJSON_AddItemToObject(o, "modality",
    modality ? cJSON_CreateString(modality) : cJSON_CreateNull());
  cJSON_AddStringToObject(o, "kind", kind);
  cJSON_AddItemToObject(o, "sources", cJSON_CreateArray());
  /* records_geocoded starts as a number and is summed in place; replaced by
   * null wholesale if the tally query itself failed (never a guessed 0). */
  cJSON_AddNumberToObject(o, "records_geocoded", 0);
  return o;
}

static void bump_records(cJSON *layer, long long n) {
  cJSON *r = cJSON_GetObjectItem(layer, "records_geocoded");
  if (r && cJSON_IsNumber(r)) cJSON_SetNumberValue(r, r->valuedouble + (double)n);
}

/* A source reference for a source id that may not be in the registry at all
 * (retired collector, pipeline-synthetic id) — the row is still in the
 * database and still reachable, so it is still listed, with nulls where the
 * registry has nothing to say. */
static void add_bare_source_ref(cJSON *sources, const char *id) {
  const src_meta *m = src_meta_get(id);
  if (m) { add_source_ref(sources, m); return; }
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", id);
  cJSON_AddNullToObject(o, "name");
  cJSON_AddNullToObject(o, "type");
  cJSON_AddNullToObject(o, "free");
  cJSON_AddItemToArray(sources, o);
}

char *miscapi_list_layers(db_handle *db) {
  cJSON *layers = cJSON_CreateArray();

  /* 1. Curated layers, in table order — the taxonomy is emitted whole, even
   * where a layer has no members yet: an empty curated layer is a promise a
   * client can render greyed, not a secret. */
  int nt = layertab_count();
  for (int t = 0; t < nt; t++) {
    const layer_row *r = layertab_at(t);
    cJSON_AddItemToArray(layers, make_layer(r->id, r->name, r->category,
                                            r->data_type, r->modality,
                                            "curated"));
  }

  /* 2. Every registered source lands in its resolved layer; a declared layer
   * id not in the table is synthesized on first sight (kebab → Title Case,
   * category from its first member — v1 behaviour). */
  int n = src_meta_count();
  for (int i = 0; i < n; i++) {
    const src_meta *m = src_meta_at(i);
    if (!m || !m->id) continue;
    const char *lid = layertab_layer_for_source(m->id);
    if (!lid) continue;                          /* unassigned → catch-all */
    cJSON *layer = find_layer(layers, lid);
    if (!layer) {
      char name[256]; titlecase_kebab(lid, name, sizeof name);
      layer = make_layer(lid, name, m->category, NULL, NULL, "declared");
      cJSON_AddItemToArray(layers, layer);
    }
    cJSON *srcs = cJSON_GetObjectItem(layer, "sources");
    if (!sources_has(srcs, m->id)) add_source_ref(srcs, m);
  }

  /* 3. Measured per-layer record counts + the generated catch-all layers.
   * One GROUP BY over the geocoded rows; each (source, record_type) bucket is
   * credited to the source's resolved layer, and buckets whose source
   * resolves to NO layer become `rt-<slug>` / `unassigned-geocoded` layers so
   * every geocoded row in the database is reachable from this listing. */
  int tally_ok = 0;
  if (db) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT source_id, COALESCE(record_type,''), COUNT(*)"
          "  FROM intel_items WHERE lat IS NOT NULL"
          " GROUP BY source_id, record_type", -1, &s, NULL) == SQLITE_OK) {
      tally_ok = 1;
      while (sqlite3_step(s) == SQLITE_ROW) {
        const char *sid = (const char *)sqlite3_column_text(s, 0);
        const char *rt  = (const char *)sqlite3_column_text(s, 1);
        long long cnt   = sqlite3_column_int64(s, 2);
        if (!sid) continue;
        const char *lid = layertab_layer_for_source(sid);
        cJSON *layer = lid ? find_layer(layers, lid) : NULL;
        if (!layer) {
          /* Catch-all. rt may be "" (record_type NULL/empty). */
          char gid[160];
          if (rt && rt[0]) {
            char slug[128]; layertab_rt_slug(rt, slug, sizeof slug);
            if (slug[0]) snprintf(gid, sizeof gid, "rt-%s", slug);
            else snprintf(gid, sizeof gid, "unassigned-geocoded");
          } else {
            snprintf(gid, sizeof gid, "unassigned-geocoded");
          }
          layer = find_layer(layers, gid);
          if (!layer) {
            char name[256];
            if (rt && rt[0]) titlecase_kebab(rt, name, sizeof name);
            else snprintf(name, sizeof name, "Unassigned Geocoded");
            layer = make_layer(gid, name, "uncurated",
                               (rt && rt[0]) ? rt : NULL, NULL, "generated");
            cJSON_AddItemToArray(layers, layer);
          }
          cJSON *srcs = cJSON_GetObjectItem(layer, "sources");
          if (!sources_has(srcs, sid)) add_bare_source_ref(srcs, sid);
        }
        bump_records(layer, cnt);
      }
    }
    sqlite3_finalize(s);
  }

  /* Tally failed (or no db): the counts were not obtained, so they are null,
   * not zero — a client must be able to tell "empty" from "unmeasured". */
  if (!tally_ok) {
    cJSON *layer;
    cJSON_ArrayForEach(layer, layers) {
      cJSON_ReplaceItemInObject(layer, "records_geocoded", cJSON_CreateNull());
    }
  }

  /* 4. Temporal disposition rides on every layer, as in v1. */
  cJSON *layer;
  cJSON_ArrayForEach(layer, layers) {
    cJSON *iid = cJSON_GetObjectItem(layer, "id");
    apply_temporal(layer, iid->valuestring);
  }

  char *js = cJSON_PrintUnformatted(layers);
  cJSON_Delete(layers);
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
