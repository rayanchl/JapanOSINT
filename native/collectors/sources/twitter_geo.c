/* collectors/social/sources/twitter_geo.c — port of
 * server/src/collectors/twitterGeo.js
 *
 * Post-cutover the emitted FeatureCollection comes PURELY from
 * stmtSelectGeocoded.all() -> rows.map(rowToFeature). This file runs that
 * SELECT (its columns and filter verbatim; its ORDER/LIMIT replaced by a
 * keyset walk, see g_tg_batch below) against ctx->db->h and rebuilds each Point Feature with the
 * exact same geometry and properties key order as the JS row mapper, emitting
 * through the shared geojson sink (which derives uid/title/record_type).
 *
 * OUT OF SCOPE (ancillary write side-effect, not part of emitted features):
 * the JS collect() also runs tryTwitterAPI()/tryMastodonPublic() which
 * upsertPosts(...) into the separate social-posts store. That write side is
 * dropped here exactly like other ancillary stores in this port (it does not
 * influence the FeatureCollection the source returns). _meta is also dropped.
 */
#include "core/dbutil.h"
#include "source.h"
#include "core/db.h"
#include "lib/geojson.h"
#include "lib/jocore.h"
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The SELECT from twitterGeo.js stmtSelectGeocoded, walked on a keyset. Column indices:
 * 0 post_uid 1 platform 2 author 3 text 4 url 5 lat 6 lon 7 geo_source
 * 8 llm_place_name 9 fetched_at 10 properties 11 uid (the keyset tiebreak) */
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
  "    properties,\n"
  "    uid\n"
  "  FROM intel_items\n"
  "  WHERE record_type = 'post'\n"
  "    AND (source_id IN ('twitter-geo','misskey-timeline')\n"
  "         OR sub_source_id IN ('twitter','mastodon'))\n"
  "    AND lat IS NOT NULL AND lon IS NOT NULL\n"
  "    AND (?1 IS NULL OR (fetched_at, uid) > (?1, ?2))\n"
  "  ORDER BY fetched_at ASC, uid ASC\n"
  "  LIMIT ?3";

/* How many geocoded posts remain past the keyset — the disclosure's number. */
static const char *SQL_REMAINING =
  "  SELECT count(*) FROM intel_items\n"
  "  WHERE record_type = 'post'\n"
  "    AND (source_id IN ('twitter-geo','misskey-timeline')\n"
  "         OR sub_source_id IN ('twitter','mastodon'))\n"
  "    AND lat IS NOT NULL AND lon IS NOT NULL\n"
  "    AND (fetched_at, uid) > (?1, ?2)";

/* THE WHOLE TABLE OVER A CYCLE, NOT ITS NEWEST 5,000 (2026-11-24, audit round
 * two's last open item). The JS original read `ORDER BY fetched_at DESC LIMIT
 * 5000`, so once more than 5,000 geocoded posts existed every older one fell
 * out of this layer for good, silently — house rule 2. Reading everything
 * every 600 s is not the answer either: it re-upserts the whole table each
 * run. So the read walks FORWARD on a keyset of (fetched_at, uid), 5,000 a
 * run, storing where it stopped, and once caught up it only reads what
 * arrived since — the window_days shape: backfill and live tail in one row.
 * A post re-fetched later moves past the keyset and is re-read, which is the
 * update it should be. `uid` is the tiebreak because many posts share one
 * fetched_at (a batch), and a keyset on fetched_at alone would skip the rest
 * of a tie at a slice boundary. While the walk is behind, a truncation notice
 * says how many posts it has not reached yet. */
static int g_tg_batch = 5000;   /* a variable so the unit test can shrink it */


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

  /* The stored position: "k=<fetched_at>\t<uid>". A token in any other shape
   * is treated as absent, as source.h asks — the walk then starts over. */
  char *pos_ts = NULL; const char *pos_uid = NULL;
  if (ctx->resume_in && !strncmp(ctx->resume_in, "k=", 2)) {
    pos_ts = strdup(ctx->resume_in + 2);
    char *tab = pos_ts ? strchr(pos_ts, '\t') : NULL;
    if (tab) { *tab = 0; pos_uid = tab + 1; }
    else { free(pos_ts); pos_ts = NULL; }
  }
  if (pos_ts) {
    sqlite3_bind_text(s, 1, pos_ts, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, pos_uid, -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(s, 3, g_tg_batch);
  char *last_ts = NULL, *last_uid = NULL;
  int nrows = 0;

  cJSON *features = cJSON_CreateArray();
  int rc;
  while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
    const char *post_uid   = db_ctext(s, 0);
    const char *platform   = db_ctext(s, 1);
    const char *author     = db_ctext(s, 2);
    const char *text       = db_ctext(s, 3);
    const char *url        = db_ctext(s, 4);
    double lat = sqlite3_column_double(s, 5);
    double lon = sqlite3_column_double(s, 6);
    const char *geo_source = db_ctext(s, 7);
    const char *llm_place  = db_ctext(s, 8);
    const char *fetched_at = db_ctext(s, 9);
    const char *row_uid    = db_ctext(s, 11);
    nrows++;
    free(last_ts); free(last_uid);
    last_ts  = fetched_at ? strdup(fetched_at) : NULL;
    last_uid = row_uid ? strdup(row_uid) : NULL;

    cJSON *f = gj_point_feature(lon, lat);

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
  free(pos_ts);
  if (failed) { cJSON_Delete(features); free(last_ts); free(last_uid); return -1; }

  int n = geojson_emit_features(sink, "twitter-geo", features);
  cJSON_Delete(features);

  /* Advance only past what was emitted. A run that read nothing keeps the
   * position it had (the walk is caught up, not finished: posts keep coming). */
  if (n >= 0 && last_ts && last_uid) {
    if (ctx->resume_out) {
      size_t L = strlen(last_ts) + strlen(last_uid) + 4;
      char *tok = malloc(L);
      if (tok) { snprintf(tok, L, "k=%s\t%s", last_ts, last_uid);
                 ctx->resume_out(ctx, tok); free(tok); }
    }
    if (nrows >= g_tg_batch) {
      long rem = -1;
      sqlite3_stmt *c;
      if (sqlite3_prepare_v2(ctx->db->h, SQL_REMAINING, -1, &c, NULL) == SQLITE_OK) {
        sqlite3_bind_text(c, 1, last_ts, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(c, 2, last_uid, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(c) == SQLITE_ROW) rem = (long)sqlite3_column_int64(c, 0);
        sqlite3_finalize(c);
      }
      if (rem > 0)
        jo_truncation_notice_ex(sink, "twitter-geo", NULL, nrows, nrows + rem,
          "this layer reads the geocoded posts oldest-first, a slice a run, and "
          "has not reached the rest yet",
          ctx->resume_out
            ? "none needed: the next run resumes after the last post read here"
            : "run it from the scheduler, which stores the position; a run "
              "without one starts from the oldest post every time",
          NULL);
    }
  }
  free(last_ts); free(last_uid);
  return n >= 0 ? 0 : -1;
}

static const source_def twitter_geo_def = {
  .id = "twitter-geo", .collector = "social",
  .name = "Twitter/X Geo Posts",
  .name_ja = "Twitter/X ジオ投稿",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(twitter_geo_def)
