/* collectors/cyber/sources/shodan_japan.c
 * Port of server/src/collectors/shodanJapan.js.
 * Shodan host/search?query=country:JP (key in querystring) → FeatureCollection.
 * Gated on SHODAN_API_KEY. No seed. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* m.<k> as string, or null. */
static cJSON *str_or_null(cJSON *m, const char *k) {
  cJSON *v = cJSON_GetObjectItem(m, k);
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  return cJSON_CreateNull();
}

/* m.product || m._shodan.module || null */
static cJSON *product_of(cJSON *m) {
  cJSON *v = cJSON_GetObjectItem(m, "product");
  if (v && cJSON_IsString(v) && v->valuestring[0])
    return cJSON_CreateString(v->valuestring);
  cJSON *sh = cJSON_GetObjectItem(m, "_shodan");
  if (sh && cJSON_IsObject(sh)) {
    cJSON *mod = cJSON_GetObjectItem(sh, "module");
    if (mod && cJSON_IsString(mod) && mod->valuestring[0])
      return cJSON_CreateString(mod->valuestring);
  }
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *k = getenv("SHODAN_API_KEY");
  if (!k || !*k) {
    fprintf(stderr, "[shodan-japan] gated (no SHODAN_API_KEY)\n");
    return 0;
  }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.shodan.io/shodan/host/search?key=%s&query=country%%3AJP", k);
  cJSON *data = feed_get_json(ctx->http, url, 20000);
  cJSON *matches = data ? cJSON_GetObjectItem(data, "matches") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(matches)) {
    cJSON *m;
    cJSON_ArrayForEach(m, matches) {
      cJSON *loc = cJSON_GetObjectItem(m, "location");
      double lon, lat;
      if (!(loc &&
            jo_num_of(cJSON_GetObjectItem(loc, "longitude"), &lon) &&
            jo_num_of(cJSON_GetObjectItem(loc, "latitude"), &lat)))
        continue;

      cJSON *f = gj_point_feature(lon, lat);

      cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
      cJSON *ipv = cJSON_GetObjectItem(m, "ip_str");
      cJSON *prtv = cJSON_GetObjectItem(m, "port");
      const char *ipstr = (ipv && cJSON_IsString(ipv)) ? ipv->valuestring : "";
      char idb[96];
      if (prtv && cJSON_IsNumber(prtv))
        snprintf(idb, sizeof idb, "%s:%g", ipstr, prtv->valuedouble);
      else if (prtv && cJSON_IsString(prtv))
        snprintf(idb, sizeof idb, "%s:%s", ipstr, prtv->valuestring);
      else
        snprintf(idb, sizeof idb, "%s:", ipstr);
      cJSON_AddStringToObject(p, "id", idb);
      cJSON_AddItemToObject(p, "ip",
        ipv ? cJSON_Duplicate(ipv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "port",
        prtv ? cJSON_Duplicate(prtv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "product", product_of(m));
      cJSON_AddItemToObject(p, "transport", str_or_null(m, "transport"));
      cJSON_AddItemToObject(p, "org", str_or_null(m, "org"));
      cJSON *cityv = loc ? cJSON_GetObjectItem(loc, "city") : NULL;
      cJSON_AddItemToObject(p, "city",
        (cityv && cJSON_IsString(cityv) && cityv->valuestring[0])
          ? cJSON_CreateString(cityv->valuestring) : cJSON_CreateNull());
      cJSON *hn = cJSON_GetObjectItem(m, "hostnames");
      cJSON_AddItemToObject(p, "hostnames",
        (hn && cJSON_IsArray(hn)) ? cJSON_Duplicate(hn, 1)
                                  : cJSON_CreateArray());
      cJSON_AddStringToObject(p, "source", "shodan_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[shodan-japan] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def shodan_japan_def = {
  .id = "shodan-japan", .collector = "cyber",
  .name = "Shodan Japan IoT Devices", .name_ja = "Shodan \xE6\x97\xA5\xE6\x9C\xACIoT\xE3\x83\x87\xE3\x83\x90\xE3\x82\xA4\xE3\x82\xB9",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(shodan_japan_def)
