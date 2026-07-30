/* collectors/cyber/sources/atlas_jp.c
 * Port of server/src/collectors/atlasJp.js.
 * RIPE Atlas public API (key-free), connected probes in JP, paginated up to
 * MAX_PAGES → FeatureCollection. Honest empty on failure. No seed. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PAGES 12   /* exhaustive-ok: page-walk runaway guard */

static int num_of(cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

/* p.<k> ?? null  (number or string passthrough) */
static cJSON *nn(cJSON *p, const char *k) {
  cJSON *v = cJSON_GetObjectItem(p, k);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  char url[768];
  snprintf(url, sizeof url,
    "https://atlas.ripe.net/api/v2/probes/?country_code=JP&status=1"
    "&page_size=100&fields=id,address_v4,asn_v4,asn_v6,prefix_v4,latitude,"
    "longitude,status_name,is_anchor,first_connected");
  int pages = 0, gotAny = 0;
  char nexturl[1024];

  while (url[0] && pages < MAX_PAGES) {
    cJSON *data = feed_get_json(ctx->http, url, 15000);
    cJSON *results = data ? cJSON_GetObjectItem(data, "results") : NULL;
    if (!data || !cJSON_IsArray(results)) {
      if (data) cJSON_Delete(data);
      break;
    }
    gotAny = 1;
    cJSON *pr;
    cJSON_ArrayForEach(pr, results) {
      double lat, lon;
      if (!num_of(cJSON_GetObjectItem(pr, "latitude"), &lat)) continue;
      if (!num_of(cJSON_GetObjectItem(pr, "longitude"), &lon)) continue;
      if (lat == 0 && lon == 0) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      cJSON_AddItemToObject(p, "probe_id", nn(pr, "id"));
      cJSON_AddItemToObject(p, "asn_v4", nn(pr, "asn_v4"));
      cJSON_AddItemToObject(p, "asn_v6", nn(pr, "asn_v6"));
      cJSON_AddItemToObject(p, "prefix_v4", nn(pr, "prefix_v4"));
      cJSON_AddItemToObject(p, "address_v4", nn(pr, "address_v4"));
      cJSON_AddItemToObject(p, "status", nn(pr, "status_name"));
      cJSON *anch = cJSON_GetObjectItem(pr, "is_anchor");
      cJSON_AddBoolToObject(p, "is_anchor",
        anch ? cJSON_IsTrue(anch) : 0);
      cJSON *fc = cJSON_GetObjectItem(pr, "first_connected");
      if (fc && cJSON_IsNumber(fc)) {
        time_t t = (time_t)fc->valuedouble;
        struct tm gt;
        gmtime_r(&t, &gt);
        char iso[32];
        snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
                 gt.tm_year + 1900, gt.tm_mon + 1, gt.tm_mday,
                 gt.tm_hour, gt.tm_min, gt.tm_sec);
        cJSON_AddStringToObject(p, "first_connected", iso);
      } else {
        cJSON_AddNullToObject(p, "first_connected");
      }
      cJSON *idv = cJSON_GetObjectItem(pr, "id");
      char link[96];
      if (idv && cJSON_IsNumber(idv))
        snprintf(link, sizeof link,
                 "https://atlas.ripe.net/probes/%g/", idv->valuedouble);
      else if (idv && cJSON_IsString(idv))
        snprintf(link, sizeof link,
                 "https://atlas.ripe.net/probes/%s/", idv->valuestring);
      else
        snprintf(link, sizeof link, "https://atlas.ripe.net/probes/undefined/");
      cJSON_AddStringToObject(p, "link", link);
      cJSON_AddStringToObject(p, "source", "ripe_atlas_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }

    cJSON *next = cJSON_GetObjectItem(data, "next");
    if (next && cJSON_IsString(next) && next->valuestring[0]) {
      snprintf(nexturl, sizeof nexturl, "%s", next->valuestring);
      snprintf(url, sizeof url, "%s", nexturl);
    } else {
      url[0] = '\0';
    }
    pages++;
    cJSON_Delete(data);
  }

  if (!gotAny) {
    cJSON_Delete(features);
    fprintf(stderr, "[atlas-jp] unavailable\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[atlas-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def atlas_jp_def = {
  .id = "atlas-jp", .collector = "cyber",
  .name = "RIPE Atlas Japan Probes",
  .name_ja = "RIPE Atlas \xE6\x97\xA5\xE6\x9C\xAC\xE3\x83\x97\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x96",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(atlas_jp_def)
