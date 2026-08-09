/* collectors/cyber/sources/leakix_jp.c
 * Port of server/src/collectors/leakixJp.js (createThreatIntelCollector).
 * env LEAKIX_API_KEY → service search country:"JP", slice 200, geoip point
 * when the record has one and no geometry at all when it does not (the Tokyo
 * fallback this was ported with stacked every unlocated host on one pin). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>

/* URL = base + encodeURIComponent('country:"JP"') */
#define LEAKIX_URL "https://leakix.net/search?scope=service&q=country%3A%22JP%22"

/* JS `a?.b?.c || null` → child or JSON null */
static cJSON *opt2(cJSON *o, const char *k1, const char *k2) {
  cJSON *a = o ? cJSON_GetObjectItem(o, k1) : NULL;
  cJSON *b = a ? cJSON_GetObjectItem(a, k2) : NULL;
  if (!b || (cJSON_IsString(b) && !b->valuestring[0]) || cJSON_IsNull(b))
    return cJSON_CreateNull();
  return cJSON_Duplicate(b, 1);
}
static cJSON *opt3(cJSON *o, const char *k1, const char *k2, const char *k3) {
  cJSON *a = o ? cJSON_GetObjectItem(o, k1) : NULL;
  cJSON *b = a ? cJSON_GetObjectItem(a, k2) : NULL;
  cJSON *c = b ? cJSON_GetObjectItem(b, k3) : NULL;
  if (!c || (cJSON_IsString(c) && !c->valuestring[0]) || cJSON_IsNull(c))
    return cJSON_CreateNull();
  return cJSON_Duplicate(c, 1);
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  char keyh[256];
  snprintf(keyh, sizeof keyh, "api-key: %s", key);
  const char *hdrs[] = { keyh, "accept: application/json", NULL };
  cJSON *arr = feed_get_json_h(ctx->http, LEAKIX_URL, hdrs, 15000);
  if (!arr) return NULL;

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *s;
    cJSON_ArrayForEach(s, arr) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      cJSON *geoip = cJSON_GetObjectItem(s, "geoip");
      cJSON *lonv = geoip ? cJSON_GetObjectItem(geoip, "longitude") : NULL;
      cJSON *latv = geoip ? cJSON_GetObjectItem(geoip, "latitude") : NULL;
      /* A host whose GeoIP record carries no position used to be pinned to
       * Tokyo (35.6895,139.6917) — one of the residual add_tokyo_geom sites,
       * and the reason unlocated LeakIX hosts stacked on one point. Emit no
       * geometry instead; the row is still useful without a map pin. */
      int has = lonv && cJSON_IsNumber(lonv) && latv && cJSON_IsNumber(latv);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      if (has) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lonv->valuedouble));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(latv->valuedouble));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      } else {
        cJSON_AddNullToObject(f, "geometry");
      }

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      jo_put_or_null(pr, "ip", s, "ip");
      jo_put_or_null(pr, "host", s, "host");
      jo_put_or_null(pr, "port", s, "port");
      jo_put_or_null(pr, "protocol", s, "protocol");
      cJSON_AddItemToObject(pr, "service_name", opt3(s, "service", "software", "name"));
      cJSON_AddItemToObject(pr, "service_version", opt3(s, "service", "software", "version"));
      jo_put_or_null(pr, "tags", s, "tags");
      jo_put_or_null(pr, "events", s, "events");
      cJSON_AddItemToObject(pr, "leak_severity", opt2(s, "leak", "severity"));
      cJSON_AddItemToObject(pr, "leak_type", opt2(s, "leak", "type"));
      jo_put_or_null(pr, "time", s, "time");
      cJSON_AddItemToObject(pr, "country", opt2(s, "geoip", "country_name"));
      cJSON_AddItemToObject(pr, "city", opt2(s, "geoip", "city_name"));
      cJSON_AddItemToObject(pr, "asn", opt2(s, "network", "asn"));
      cJSON_AddItemToObject(pr, "as_name", opt2(s, "network", "organization_name"));
      cJSON_AddStringToObject(pr, "source", "leakix");
      if (has) {
        /* LeakIX positions are GeoIP: a city/ISP area, not the host. */
        cJSON_AddStringToObject(pr, "geo_provenance", "leakix-geoip");
        cJSON_AddStringToObject(pr, "geo_precision", "city");
        cJSON_AddBoolToObject(pr, "geo_uncertain", 1);
      }
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(arr);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "LEAKIX_API_KEY", NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def leakix_jp_def = {
  .id = "leakix-jp", .collector = "cyber",
  .name = "LeakIX (JP services)", .name_ja = "LeakIX (日本サービス)",
   .update_interval_sec = 7200, .run = run };
REGISTER_SOURCE(leakix_jp_def)
