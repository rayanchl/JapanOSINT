/* collectors/cyber/sources/leakix_jp.c
 * Port of server/src/collectors/leakixJp.js (createThreatIntelCollector).
 * env LEAKIX_API_KEY → service search country:"JP", slice 200, geoip point
 * or TOKYO fallback. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
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
static void put(cJSON *p, const char *k, cJSON *r, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(r, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
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
      int has = lonv && cJSON_IsNumber(lonv) && latv && cJSON_IsNumber(latv);
      double lon = has ? lonv->valuedouble : TOKYO_LON;
      double lat = has ? latv->valuedouble : TOKYO_LAT;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      put(pr, "ip", s, "ip");
      put(pr, "host", s, "host");
      put(pr, "port", s, "port");
      put(pr, "protocol", s, "protocol");
      cJSON_AddItemToObject(pr, "service_name", opt3(s, "service", "software", "name"));
      cJSON_AddItemToObject(pr, "service_version", opt3(s, "service", "software", "version"));
      put(pr, "tags", s, "tags");
      put(pr, "events", s, "events");
      cJSON_AddItemToObject(pr, "leak_severity", opt2(s, "leak", "severity"));
      cJSON_AddItemToObject(pr, "leak_type", opt2(s, "leak", "type"));
      put(pr, "time", s, "time");
      cJSON_AddItemToObject(pr, "country", opt2(s, "geoip", "country_name"));
      cJSON_AddItemToObject(pr, "city", opt2(s, "geoip", "city_name"));
      cJSON_AddItemToObject(pr, "asn", opt2(s, "network", "asn"));
      cJSON_AddItemToObject(pr, "as_name", opt2(s, "network", "organization_name"));
      cJSON_AddStringToObject(pr, "source", "leakix");
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
