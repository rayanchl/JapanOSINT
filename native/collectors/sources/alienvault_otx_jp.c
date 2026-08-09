/* collectors/cyber/sources/alienvault_otx_jp.c
 * Port of server/src/collectors/alienvaultOtxJp.js (createThreatIntelCollector).
 * env OTX_API_KEY → OTX pulse search q=japan, 50 recent, TOKYO points. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>

#define OTX_URL "https://otx.alienvault.com/api/v1/search/pulses?q=japan&limit=50&sort=-modified"

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  char keyh[256];
  snprintf(keyh, sizeof keyh, "X-OTX-API-KEY: %s", key);
  const char *hdrs[] = { keyh, "accept: application/json", NULL };
  cJSON *json = feed_get_json_h(ctx->http, OTX_URL, hdrs, 15000);
  if (!json) return NULL;

  cJSON *results = cJSON_GetObjectItem(json, "results");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(results)) {
    cJSON *p;
    cJSON_ArrayForEach(p, results) {
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
       * (35.6895, 139.6917). What this source reports has no location,
       * and stacking every row on one pin is the fabrication the
       * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
       * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
       * lib/geojson.c handles an absent geometry correctly — do NOT
       * reintroduce a fallback coordinate. */

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      jo_put_or_null(pr, "pulse_id", p, "id");
      jo_put_or_null(pr, "title", p, "name");
      cJSON *auth = cJSON_GetObjectItem(p, "author");
      cJSON *au = auth ? cJSON_GetObjectItem(auth, "username") : NULL;
      cJSON_AddItemToObject(pr, "author",
        au ? cJSON_Duplicate(au, 1) : cJSON_CreateNull());
      jo_put_or_null(pr, "modified", p, "modified");
      jo_put_or_null(pr, "created", p, "created");
      jo_put_or_null(pr, "tlp", p, "tlp");
      jo_put_or_null(pr, "adversary", p, "adversary");
      jo_put_or_null(pr, "industries", p, "industries");
      jo_put_or_null(pr, "targeted_countries", p, "targeted_countries");
      jo_put_or_null(pr, "indicator_count", p, "indicator_count");
      jo_put_or_null(pr, "tags", p, "tags");
      cJSON *idv = cJSON_GetObjectItem(p, "id");
      if (idv && cJSON_IsString(idv) && idv->valuestring[0]) {
        char url[256];
        snprintf(url, sizeof url, "https://otx.alienvault.com/pulse/%s",
                 idv->valuestring);
        cJSON_AddStringToObject(pr, "url", url);
      } else {
        cJSON_AddItemToObject(pr, "url", cJSON_CreateNull());
      }
      cJSON_AddStringToObject(pr, "source", "otx_search_pulses");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "OTX_API_KEY", NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def alienvault_otx_jp_def = {
  .id = "alienvault-otx-jp", .collector = "cyber",
  .name = "AlienVault OTX (JP-targeted)", .name_ja = "AlienVault OTX (日本標的)",
   .update_interval_sec = 7200, .run = run };
REGISTER_SOURCE(alienvault_otx_jp_def)
