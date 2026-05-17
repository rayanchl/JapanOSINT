/* collectors/social/sources/wikipedia_ja_recent.c
 * Port of server/src/collectors/wikipediaJaRecent.js.
 * ja.wikipedia recentchanges (ns0, rclimit=200) → FeatureCollection at TOKYO.
 * Keyless. Fetch-fail → 0 features (correctness-neutral). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://ja.wikipedia.org/w/api.php?action=query&list=recentchanges&rcprop=title|user|timestamp|comment|sizes|ids&rcnamespace=0&rclimit=200&format=json&origin=*"
#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895

static void passthru(cJSON *p, const char *k, cJSON *rc, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(rc, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

/* x ?? 0  (number) */
static double n0(cJSON *rc, const char *k) {
  cJSON *v = cJSON_GetObjectItem(rc, k);
  if (v && cJSON_IsNumber(v)) return v->valuedouble;
  return 0;
}

/* encodeURIComponent: percent-encode all but A-Za-z0-9 -_.!~*'() */
static void enc_uri_comp(const char *s, char *o, size_t cap) {
  static const char *unres = "-_.!~*'()";
  size_t j = 0;
  for (; *s && j + 4 < cap; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(unres, c)) {
      o[j++] = (char)c;
    } else {
      static const char hx[] = "0123456789ABCDEF";
      o[j++] = '%'; o[j++] = hx[c >> 4]; o[j++] = hx[c & 0xF];
    }
  }
  o[j] = '\0';
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "user-agent: japanosint-collector", NULL };
  cJSON *json = feed_get_json_h(ctx->http, API_URL, hdrs, 12000);

  cJSON *query = json ? cJSON_GetObjectItem(json, "query") : NULL;
  cJSON *arr = query ? cJSON_GetObjectItem(query, "recentchanges") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(arr)) {
    int i = 0;
    cJSON *rc;
    cJSON_ArrayForEach(rc, arr) {
      if (i >= 200) break;                          /* slice(0,200) */

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "idx", i);
      passthru(p, "title", rc, "title");
      passthru(p, "user", rc, "user");
      passthru(p, "timestamp", rc, "timestamp");
      passthru(p, "comment", rc, "comment");
      passthru(p, "pageid", rc, "pageid");
      passthru(p, "revid", rc, "revid");
      passthru(p, "old_revid", rc, "old_revid");
      passthru(p, "oldlen", rc, "oldlen");
      passthru(p, "newlen", rc, "newlen");
      cJSON_AddNumberToObject(p, "delta", n0(rc, "newlen") - n0(rc, "oldlen"));
      cJSON *tv = cJSON_GetObjectItem(rc, "title");
      if (tv && cJSON_IsString(tv) && tv->valuestring[0]) {
        char repl[512];
        size_t j = 0;
        for (const char *s = tv->valuestring; *s && j + 1 < sizeof repl; s++)
          repl[j++] = (*s == ' ') ? '_' : *s;
        repl[j] = '\0';
        char enc[1536];
        enc_uri_comp(repl, enc, sizeof enc);
        char ub[1600];
        snprintf(ub, sizeof ub, "https://ja.wikipedia.org/wiki/%s", enc);
        cJSON_AddStringToObject(p, "url", ub);
      } else {
        cJSON_AddItemToObject(p, "url", cJSON_CreateNull());
      }
      cJSON_AddStringToObject(p, "source", "wp_ja_recentchanges");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  if (json) cJSON_Delete(json);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wikipedia-ja-recent] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wikipedia_ja_recent_def = {
  .id = "wikipedia-ja-recent", .collector = "social",
  .name = "Wikipedia (ja) recent", .name_ja = "ウィキペディア最新",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(wikipedia_ja_recent_def)
