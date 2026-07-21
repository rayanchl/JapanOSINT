/* collectors/social/sources/niconico_ranking.c — port of
 * server/src/collectors/niconicoRanking.js
 *
 * Post-cutover the emitted FeatureCollection comes PURELY from
 * stmtSelectGeocoded.all() -> rows.map(rowToFeature). This file runs that
 * SELECT verbatim against ctx->db->h (sub_source_id = 'niconico') and rebuilds
 * each Point Feature with the exact geometry and properties key order of the
 * JS rowToFeature(), emitting through the shared geojson sink (which derives
 * uid/title/record_type).
 *
 * OUT OF SCOPE (ancillary write side-effect, not part of emitted features):
 * the JS collect() also fetches nvapi/live ranking and upsertPosts(batch) into
 * the separate social-posts store. That write side is dropped here exactly
 * like other ancillary stores in this port (it does not influence the
 * FeatureCollection the source returns). _meta / metadata are also dropped.
 */
#include "../../source.h"
#include "../../core/db.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Verbatim SELECT from niconicoRanking.js stmtSelectGeocoded. Column indices:
 * 0 post_uid 1 platform 2 author 3 text 4 title 5 url 6 lat 7 lon
 * 8 geo_source 9 llm_place_name 10 fetched_at 11 properties */
static const char *SQL =
  "  SELECT\n"
  "    substr(uid, instr(uid, '|') + 1)                AS post_uid,\n"
  "    COALESCE(sub_source_id, source_id)              AS platform,\n"
  "    author,\n"
  "    body                                            AS text,\n"
  "    title,\n"
  "    link                                            AS url,\n"
  "    lat, lon,\n"
  "    geom_source                                     AS geo_source,\n"
  "    json_extract(properties, '$.llm_place_name')    AS llm_place_name,\n"
  "    fetched_at,\n"
  "    properties\n"
  "    FROM intel_items\n"
  "   WHERE record_type = 'post'\n"
  "     AND sub_source_id = 'niconico'\n"
  "     AND lat IS NOT NULL AND lon IS NOT NULL\n"
  "   ORDER BY fetched_at DESC\n"
  "   LIMIT 5000";

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}

/* JS String.prototype.slice(0,280); cut on a UTF-8 boundary. */
static void slice280(const char *src, char *out, size_t outsz) {
  size_t chars = 0, bi = 0;
  if (!src) { out[0] = 0; return; }
  while (src[bi] && chars < 280 && bi + 4 < outsz) {
    unsigned char c = (unsigned char)src[bi];
    size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
    for (size_t k = 0; k < adv && src[bi]; k++) { out[bi] = src[bi]; bi++; }
    chars++;
  }
  out[bi] = 0;
}

/* JS `props.k || null`: string-truthy (non-empty string) -> that string,
 * anything falsy -> null. */
static cJSON *prop_str_or_null(cJSON *props, const char *k) {
  cJSON *v = props ? cJSON_GetObjectItem(props, k) : NULL;
  if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  return cJSON_CreateNull();
}

/* JS `props.k ?? null`: emit the stored number when present (incl. 0); null on
 * null/undefined (missing key or JSON null). Non-number values fall through to
 * their natural cJSON form, matching `??` keeping a present non-nullish value. */
static cJSON *prop_num_or_null(cJSON *props, const char *k) {
  cJSON *v = props ? cJSON_GetObjectItem(props, k) : NULL;
  if (!v || cJSON_IsNull(v)) return cJSON_CreateNull();
  return cJSON_Duplicate(v, 1);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(ctx->db->h, SQL, -1, &s, NULL) != SQLITE_OK)
    return -1;

  cJSON *features = cJSON_CreateArray();
  int rc;
  while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    const char *post_uid   = ctext(s, 0);
    const char *platform   = ctext(s, 1);
    const char *author     = ctext(s, 2);
    const char *text       = ctext(s, 3);
    const char *title      = ctext(s, 4);
    const char *url        = ctext(s, 5);
    double lat = sqlite3_column_double(s, 6);
    double lon = sqlite3_column_double(s, 7);
    const char *geo_source = ctext(s, 8);
    const char *llm_place  = ctext(s, 9);
    const char *fetched_at = ctext(s, 10);
    const char *props_json = ctext(s, 11);

    /* safeJson(row.properties): parse, {} on null/parse-fail */
    cJSON *props = (props_json && *props_json) ? cJSON_Parse(props_json) : NULL;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *coords = cJSON_CreateArray();
    cJSON_AddItemToArray(coords, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(coords, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", coords);
    cJSON_AddItemToObject(f, "geometry", g);

    /* properties key order (niconicoRanking.js rowToFeature):
     * id, platform, kind, rank, username, title, text, url, thumbnail_url,
     * view_count, viewer_count, comment_count, duration_s, timestamp, area,
     * source */
    cJSON *p = cJSON_CreateObject();
    /* id := row.post_uid (substr -> always a string) */
    cJSON_AddItemToObject(p, "id",
      post_uid ? cJSON_CreateString(post_uid) : cJSON_CreateNull());
    /* platform := COALESCE(sub_source_id, source_id) -> non-null */
    cJSON_AddItemToObject(p, "platform",
      platform ? cJSON_CreateString(platform) : cJSON_CreateNull());
    /* kind := props.kind || null */
    cJSON_AddItemToObject(p, "kind", prop_str_or_null(props, "kind"));
    /* rank := props.rank ?? null */
    cJSON_AddItemToObject(p, "rank", prop_num_or_null(props, "rank"));
    /* username := row.author */
    cJSON_AddItemToObject(p, "username",
      author ? cJSON_CreateString(author) : cJSON_CreateNull());
    /* title := row.title */
    cJSON_AddItemToObject(p, "title",
      title ? cJSON_CreateString(title) : cJSON_CreateNull());
    /* text := row.text?.slice(0,280) || null (empty -> null) */
    if (text && *text) {
      size_t blen = strlen(text);
      char *buf = (char *)malloc(blen + 8);
      slice280(text, buf, blen + 8);
      if (*buf) cJSON_AddItemToObject(p, "text", cJSON_CreateString(buf));
      else cJSON_AddItemToObject(p, "text", cJSON_CreateNull());
      free(buf);
    } else {
      cJSON_AddItemToObject(p, "text", cJSON_CreateNull());
    }
    /* url := row.url */
    cJSON_AddItemToObject(p, "url",
      url ? cJSON_CreateString(url) : cJSON_CreateNull());
    /* thumbnail_url := props.thumbnail_url || null */
    cJSON_AddItemToObject(p, "thumbnail_url",
      prop_str_or_null(props, "thumbnail_url"));
    /* view_count/viewer_count/comment_count/duration_s := props.X ?? null */
    cJSON_AddItemToObject(p, "view_count",
      prop_num_or_null(props, "view_count"));
    cJSON_AddItemToObject(p, "viewer_count",
      prop_num_or_null(props, "viewer_count"));
    cJSON_AddItemToObject(p, "comment_count",
      prop_num_or_null(props, "comment_count"));
    cJSON_AddItemToObject(p, "duration_s",
      prop_num_or_null(props, "duration_s"));
    /* timestamp := row.fetched_at */
    cJSON_AddItemToObject(p, "timestamp",
      fetched_at ? cJSON_CreateString(fetched_at) : cJSON_CreateNull());
    /* area := row.llm_place_name (json_extract -> nullable) */
    cJSON_AddItemToObject(p, "area",
      llm_place ? cJSON_CreateString(llm_place) : cJSON_CreateNull());
    /* source := geo_source==='llm_gsi' ? 'niconico+llm'
     *                                  : `niconico_${geo_source}`
     * JS template `${null}` -> literal "null" when geo_source is NULL. */
    {
      char src[256];
      if (geo_source && strcmp(geo_source, "llm_gsi") == 0)
        snprintf(src, sizeof src, "niconico+llm");
      else
        snprintf(src, sizeof src, "niconico_%s",
                 geo_source ? geo_source : "null");
      cJSON_AddStringToObject(p, "source", src);
    }
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);

    cJSON_Delete(props);
  }
  int failed = (rc != SQLITE_DONE);
  sqlite3_finalize(s);
  if (failed) { cJSON_Delete(features); return -1; }

  int n = geojson_emit_features(sink, "niconico-ranking", features);
  cJSON_Delete(features);
  return n >= 0 ? 0 : -1;
}

static const source_def niconico_ranking_def = {
  .id = "niconico-ranking", .collector = "social",
  .name = "Niconico ranking",
  .name_ja = "ニコニコ ランキング",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(niconico_ranking_def)
