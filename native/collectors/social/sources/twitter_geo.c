/* collectors/social/sources/twitter_geo.c — port of
 * server/src/collectors/twitterGeo.js
 *
 * Post-cutover the emitted FeatureCollection comes PURELY from
 * stmtSelectGeocoded.all() -> rows.map(rowToFeature). This file runs that
 * SELECT verbatim against ctx->db->h and rebuilds each Point Feature with the
 * exact same geometry and properties key order as the JS row mapper, emitting
 * through the shared geojson sink (which derives uid/title/record_type).
 *
 * OUT OF SCOPE (ancillary write side-effect, not part of emitted features):
 * the JS collect() also runs tryTwitterAPI()/tryMastodonPublic() which
 * upsertPosts(...) into the separate social-posts store. That write side is
 * dropped here exactly like other ancillary stores in this port (it does not
 * influence the FeatureCollection the source returns). _meta is also dropped.
 */
#include "../../../source.h"
#include "../../../core/db.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include "../../../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Verbatim SELECT from twitterGeo.js stmtSelectGeocoded. Column indices:
 * 0 post_uid 1 platform 2 author 3 text 4 url 5 lat 6 lon 7 geo_source
 * 8 llm_place_name 9 fetched_at 10 properties */
static const char *SQL =
  "  SELECT\n"
  "    substr(uid, instr(uid, '|') + 1)                  AS post_uid,\n"
  "    COALESCE(sub_source_id, source_id)                AS platform,\n"
  "    author,\n"
  "    body                                              AS text,\n"
  "    link                                              AS url,\n"
  "    lat, lon,\n"
  "    geom_source                                       AS geo_source,\n"
  "    json_extract(properties, '$.llm_place_name')      AS llm_place_name,\n"
  "    fetched_at,\n"
  "    properties\n"
  "  FROM intel_items\n"
  "  WHERE record_type = 'post'\n"
  "    AND (source_id IN ('twitter-geo','misskey-timeline')\n"
  "         OR sub_source_id IN ('twitter','mastodon'))\n"
  "    AND lat IS NOT NULL AND lon IS NOT NULL\n"
  "  ORDER BY fetched_at DESC\n"
  "  LIMIT 5000";

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}

/* JS String.prototype.slice(0,280): keep <=280 leading characters. The uid is
 * derived from properties.id (NATIVE_ID_KEYS) so this truncation never affects
 * the uid; cut on a UTF-8 char boundary so we never split a multibyte rune. */
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
    const char *url        = ctext(s, 4);
    double lat = sqlite3_column_double(s, 5);
    double lon = sqlite3_column_double(s, 6);
    const char *geo_source = ctext(s, 7);
    const char *llm_place  = ctext(s, 8);
    const char *fetched_at = ctext(s, 9);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *coords = cJSON_CreateArray();
    cJSON_AddItemToArray(coords, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(coords, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", coords);
    cJSON_AddItemToObject(f, "geometry", g);

    /* properties key order (twitterGeo.js rowToFeature):
     * id, platform, username, text, url, timestamp, area, source */
    cJSON *p = cJSON_CreateObject();
    /* id := r.post_uid (substr -> always a string) */
    cJSON_AddItemToObject(p, "id",
      post_uid ? cJSON_CreateString(post_uid) : cJSON_CreateNull());
    /* platform := COALESCE(sub_source_id, source_id) -> always non-null */
    cJSON_AddItemToObject(p, "platform",
      platform ? cJSON_CreateString(platform) : cJSON_CreateNull());
    /* username := r.author (nullable) */
    cJSON_AddItemToObject(p, "username",
      author ? cJSON_CreateString(author) : cJSON_CreateNull());
    /* text := r.text?.slice(0,280) || null  (empty string -> null) */
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
    /* url := r.url (nullable) */
    cJSON_AddItemToObject(p, "url",
      url ? cJSON_CreateString(url) : cJSON_CreateNull());
    /* timestamp := r.fetched_at */
    cJSON_AddItemToObject(p, "timestamp",
      fetched_at ? cJSON_CreateString(fetched_at) : cJSON_CreateNull());
    /* area := r.llm_place_name (json_extract -> nullable) */
    cJSON_AddItemToObject(p, "area",
      llm_place ? cJSON_CreateString(llm_place) : cJSON_CreateNull());
    /* source := geo_source==='llm_gsi' ? `${platform}+llm`
     *                                  : `${platform}_${geo_source}`
     * JS template literal: a NULL platform/geo_source stringifies as the
     * literal text "null" (template `${null}`). */
    {
      const char *plat_s = platform ? platform : "null";
      char src[512];
      if (geo_source && strcmp(geo_source, "llm_gsi") == 0)
        snprintf(src, sizeof src, "%s+llm", plat_s);
      else
        snprintf(src, sizeof src, "%s_%s", plat_s,
                 geo_source ? geo_source : "null");
      cJSON_AddStringToObject(p, "source", src);
    }
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  int failed = (rc != SQLITE_DONE);
  sqlite3_finalize(s);
  if (failed) { cJSON_Delete(features); return -1; }

  int n = geojson_emit_features(sink, "twitter-geo", features);
  cJSON_Delete(features);
  return n >= 0 ? 0 : -1;
}

static const source_def twitter_geo_def = {
  .id = "twitter-geo", .collector = "social",
  .name = "Twitter/X Geo Posts",
  .name_ja = "Twitter/X ジオ投稿",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(twitter_geo_def)
