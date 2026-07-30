/* collectors/cyber/sources/abuseipdb_jp.c
 * Port of server/src/collectors/abuseipdbJp.js (createThreatIntelCollector).
 * env ABUSEIPDB_API_KEY → blacklist, JP-filter, slice 500, TOKYO points.
 * REFERENCE source.c for the THREATINTEL family. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _satelliteSeeds TOKYO (== feodo_tracker_jp.c). */
#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  const char *lim = getenv("ABUSEIPDB_LIMIT");
  long limit = (lim && *lim) ? strtol(lim, NULL, 10) : 10000;
  char url[256];
  snprintf(url, sizeof url,
    "https://api.abuseipdb.com/api/v2/blacklist?confidenceMinimum=90&limit=%ld",
    limit);
  char keyh[256];
  snprintf(keyh, sizeof keyh, "Key: %s", key);
  const char *hdrs[] = { keyh, "Accept: application/json", NULL };
  cJSON *json = feed_get_json_h(ctx->http, url, hdrs, 15000);
  if (!json) return NULL;

  cJSON *data = cJSON_GetObjectItem(json, "data");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(data)) {
    cJSON *r;
    cJSON_ArrayForEach(r, data) {
      cJSON *ccv = cJSON_GetObjectItem(r, "countryCode");
      const char *cc = (ccv && cJSON_IsString(ccv)) ? ccv->valuestring : "";
      if (!((cc[0]=='J'||cc[0]=='j') && (cc[1]=='P'||cc[1]=='p') && !cc[2]))
        continue;
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();               /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "idx", i);
      cJSON *ipv = cJSON_GetObjectItem(r, "ipAddress");
      cJSON_AddItemToObject(p, "ip",
        ipv ? cJSON_Duplicate(ipv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "country",
        ccv ? cJSON_Duplicate(ccv, 1) : cJSON_CreateNull());
      cJSON *sc = cJSON_GetObjectItem(r, "abuseConfidenceScore");
      cJSON_AddItemToObject(p, "abuse_score",
        sc ? cJSON_Duplicate(sc, 1) : cJSON_CreateNull());
      cJSON *lr = cJSON_GetObjectItem(r, "lastReportedAt");
      cJSON_AddItemToObject(p, "last_reported",
        lr ? cJSON_Duplicate(lr, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "source", "abuseipdb");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "ABUSEIPDB_API_KEY", NULL,
                              run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def abuseipdb_jp_def = {
  .id = "abuseipdb-jp", .collector = "cyber",
  .name = "AbuseIPDB blacklist (JP)", .name_ja = "AbuseIPDB ブラックリスト 日本",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(abuseipdb_jp_def)
