/* collectors/transport/sources/odpt_train.c
 * Port of server/src/collectors/odptRealtime.js::collectOdptTrain.
 * Live ODPT v4 odpt:Train pull when an ODPT token is set; without a token
 * JS returns an honest empty FeatureCollection (no fabricated data). Mirrors
 * odpt_transport.c token idiom: ODPT_TOKEN | ODPT_CONSUMER_KEY |
 * ODPT_CHALLENGE_TOKEN. Tries the two ODPT v4 bases in order, first non-empty
 * array wins (=== odptGet). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *BASES[] = {
  "https://api.odpt.org/api/v4/",
  "https://api-challenge.odpt.org/api/v4/",
};

static const char *odpt_tok(void) {
  const char *t = getenv("ODPT_TOKEN");
  if (t && *t) return t;
  t = getenv("ODPT_CONSUMER_KEY");
  if (t && *t) return t;
  t = getenv("ODPT_CHALLENGE_TOKEN");
  if (t && *t) return t;
  return NULL;
}

/* odptGet(rdfType, tok): first base with a non-empty array. */
static cJSON *odpt_get(http_client *http, const char *rdf, const char *tok) {
  for (size_t i = 0; i < sizeof BASES / sizeof BASES[0]; i++) {
    char url[512];
    snprintf(url, sizeof url, "%s%s?acl:consumerKey=%s", BASES[i], rdf, tok);
    cJSON *d = feed_get_json(http, url, 20000);
    if (d && cJSON_IsArray(d) && cJSON_GetArraySize(d) > 0) return d;
    if (d) cJSON_Delete(d);
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = odpt_tok();
  if (!tok) {
    fprintf(stderr, "[odpt-train] gated (no ODPT token)\n");
    return 0;                       /* honest empty FeatureCollection */
  }
  cJSON *rows = odpt_get(ctx->http, "odpt:Train", tok);

  cJSON *features = cJSON_CreateArray();
  if (rows) {
    cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      cJSON *la = cJSON_GetObjectItem(r, "geo:lat");
      cJSON *lo = cJSON_GetObjectItem(r, "geo:long");
      if (!la || cJSON_IsNull(la) || !lo || cJSON_IsNull(lo)) continue;
      double lat = cJSON_IsNumber(la) ? la->valuedouble
                 : (cJSON_IsString(la) ? strtod(la->valuestring, NULL) : 0);
      double lon = cJSON_IsNumber(lo) ? lo->valuedouble
                 : (cJSON_IsString(lo) ? strtod(lo->valuestring, NULL) : 0);

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *same = cJSON_GetObjectItem(r, "owl:sameAs");
      cJSON *atid = cJSON_GetObjectItem(r, "@id");
      if (same && cJSON_IsString(same) && same->valuestring[0])
        cJSON_AddStringToObject(p, "train_uid", same->valuestring);
      else if (atid && cJSON_IsString(atid) && atid->valuestring[0])
        cJSON_AddStringToObject(p, "train_uid", atid->valuestring);
      else
        cJSON_AddItemToObject(p, "train_uid", cJSON_CreateNull());
      jo_put_str_or_null(p, "railway", r, "odpt:railway");
      cJSON *dl = cJSON_GetObjectItem(r, "odpt:delay");
      cJSON_AddItemToObject(p, "delay_sec",
        dl ? cJSON_Duplicate(dl, 1) : cJSON_CreateNull());
      jo_put_str_or_null(p, "from_station", r, "odpt:fromStation");
      jo_put_str_or_null(p, "to_station", r, "odpt:toStation");
      cJSON_AddStringToObject(p, "source", "odpt_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(rows);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[odpt-train] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def odpt_train_def = {
  .id = "odpt-train", .collector = "transport",
  .name = "ODPT Train Data", .name_ja = "ODPT 列車データ",
   .update_interval_sec = 30, .run = run };
REGISTER_SOURCE(odpt_train_def)
