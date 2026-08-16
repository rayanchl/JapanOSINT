/* collectors/cyber/sources/ooni_jp.c
 * Port of server/src/collectors/ooniJp.js (createThreatIntelCollector).
 * Keyless: OONI measurements probe_cc=JP, recent 200. Emitted WITHOUT geometry:
 * OONI returns no measurement coordinates, so we do not invent any (the JS
 * upstream stamped every row with a hardcoded Tokyo point — dropped here per the
 * no-fabricated-data rule; non-geo intel rows carry the real measurement fields). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/threatintel.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>

#define OONI_URL "https://api.ooni.io/api/v1/measurements?probe_cc=JP&limit=200&order_by=measurement_start_time&order=desc"

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *json = feed_get_json_h(ctx->http, OONI_URL, hdrs, 20000);
  if (!json) return NULL;

  cJSON *results = cJSON_GetObjectItem(json, "results");
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  if (cJSON_IsArray(results)) {
    cJSON *m;
    cJSON_ArrayForEach(m, results) {
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* no geometry: OONI gives no coordinates — emit as non-geo intel */

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON_AddNumberToObject(pr, "idx", i);
      jo_put_or_null(pr, "measurement_uid", m, "measurement_uid");
      jo_put_or_null(pr, "report_id", m, "report_id");
      jo_put_or_null(pr, "test_name", m, "test_name");
      jo_put_or_null(pr, "input", m, "input");
      jo_put_or_null(pr, "probe_cc", m, "probe_cc");
      jo_put_or_null(pr, "probe_asn", m, "probe_asn");
      jo_put_or_null(pr, "anomaly", m, "anomaly");
      jo_put_or_null(pr, "confirmed", m, "confirmed");
      jo_put_or_null(pr, "failure", m, "failure");
      /* OONI names this measurement_start_time; the JS port read a
       * "test_start_time" key that the API does not return, so every row
       * landed with published_at = NULL. Read the real key, keep the old
       * name as an alias so nothing downstream that reads it breaks. */
      jo_put_or_null(pr, "test_start_time", m, "measurement_start_time");
      jo_put_or_null(pr, "published_at", m, "measurement_start_time");
      jo_put_or_null(pr, "verification_status", m, "verification_status");
      /* scores.blocking_* is the actual censorship signal; it was dropped. */
      cJSON *sc = cJSON_GetObjectItem(m, "scores");
      if (sc) cJSON_AddItemToObject(pr, "scores", cJSON_Duplicate(sc, 1));
      jo_put_or_null(pr, "url", m, "measurement_url");
      cJSON_AddStringToObject(pr, "source", "ooni");

      /* Every row landed title-less (geojson_emit_features picks
       * title/name/name_ja/label off properties and none were set), so all
       * 200 measurements were unreadable in every UI. Build one. */
      const cJSON *tn = cJSON_GetObjectItem(m, "test_name");
      const cJSON *in = cJSON_GetObjectItem(m, "input");
      const cJSON *asn = cJSON_GetObjectItem(m, "probe_asn");
      const cJSON *an = cJSON_GetObjectItem(m, "anomaly");
      const cJSON *cf = cJSON_GetObjectItem(m, "confirmed");
      const char *tns = cJSON_IsString(tn) ? tn->valuestring : "measurement";
      const char *ins = cJSON_IsString(in) ? in->valuestring : NULL;
      const char *asns = cJSON_IsString(asn) ? asn->valuestring : NULL;
      const char *flag = cJSON_IsTrue(cf) ? "CONFIRMED BLOCK · "
                       : cJSON_IsTrue(an) ? "anomaly · " : "";
      char title[512];
      snprintf(title, sizeof title, "%s%s%s%s", flag, tns,
               ins ? " · " : "", ins ? ins : "");
      cJSON_AddStringToObject(pr, "title", title);
      char summ[256];
      snprintf(summ, sizeof summ, "OONI %s from JP%s%s", tns,
               asns ? " / " : "", asns ? asns : "");
      cJSON_AddStringToObject(pr, "summary", summ);
      cJSON_AddStringToObject(pr, "language", "en");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  cJSON_Delete(json);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ooni_jp_def = {
  .id = "ooni-jp", .collector = "cyber",
  .name = "OONI Explorer (JP)", .name_ja = "OONI Explorer 日本",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(ooni_jp_def)
