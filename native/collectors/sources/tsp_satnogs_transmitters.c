/* collectors/sources/tsp_satnogs_transmitters.c
 * SatNOGS DB transmitters — the per-satellite RF downlink/uplink registry.
 * Endpoint: https://db.satnogs.org/api/transmitters/?format=json  (keyless)
 * Emits: uuid, description, alive flag, type, uplink_low/high, downlink_low/
 *   high, downlink_drift, mode, baud, sat_id, norad_cat_id, status, service,
 *   iaru_coordination, itu_notification, frequency_violation, updated.
 * Geo: none — a transmitter record has no ground point, so has_geo stays 0 (R2).
 * parse_notes honoured: "Top-level JSON array"; "Frequencies are integer Hz
 *   (136650000 = 136.65 MHz)"; "norad_cat_id joins to celestrak-satcat".
 *   Note space_world.c already uses db.satnogs.org but only for
 *   /api/satellites/ — this is a different endpoint with different content.
 * Licence: SatNOGS DB (Libre Space Foundation) — CC-BY-SA, keyless API.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SATNOGS_TX_URL "https://db.satnogs.org/api/transmitters/?format=json"
#define MAX_ROWS 12000

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SATNOGS_TX_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[satnogs-db-transmitters] fetch/parse failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[satnogs-db-transmitters] unexpected shape\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *tx;
  cJSON_ArrayForEach(tx, doc) {
    if (n >= MAX_ROWS) break;
    const char *uuid = jo_sv(tx, "uuid");
    if (!uuid) continue;                          /* no identity -> no row */

    const char *desc  = jo_sv(tx, "description");
    const char *mode  = jo_sv(tx, "mode");
    const char *stat  = jo_sv(tx, "status");
    const char *satid = jo_sv(tx, "sat_id");

    double dl = 0, ul = 0, d;
    int has_dl = jo_num(tx, "downlink_low", &dl);
    int has_ul = jo_num(tx, "uplink_low", &ul);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "uuid", uuid);
    if (desc)  cJSON_AddStringToObject(pr, "description", desc);
    if (mode)  cJSON_AddStringToObject(pr, "mode", mode);
    if (stat)  cJSON_AddStringToObject(pr, "status", stat);
    if (satid) cJSON_AddStringToObject(pr, "sat_id", satid);
    const char *s;
    if ((s = jo_sv(tx, "type")))              cJSON_AddStringToObject(pr, "type", s);
    if ((s = jo_sv(tx, "service")))           cJSON_AddStringToObject(pr, "service", s);
    if ((s = jo_sv(tx, "iaru_coordination"))) cJSON_AddStringToObject(pr, "iaru_coordination", s);
    if ((s = jo_sv(tx, "itu_notification")))  cJSON_AddStringToObject(pr, "itu_notification", s);
    if ((s = jo_sv(tx, "updated")))           cJSON_AddStringToObject(pr, "updated", s);
    if (has_dl) cJSON_AddNumberToObject(pr, "downlink_low_hz", dl);
    if (jo_num(tx, "downlink_high", &d))  cJSON_AddNumberToObject(pr, "downlink_high_hz", d);
    if (jo_num(tx, "downlink_drift", &d)) cJSON_AddNumberToObject(pr, "downlink_drift", d);
    if (has_ul) cJSON_AddNumberToObject(pr, "uplink_low_hz", ul);
    if (jo_num(tx, "uplink_high", &d))    cJSON_AddNumberToObject(pr, "uplink_high_hz", d);
    if (jo_num(tx, "baud", &d))           cJSON_AddNumberToObject(pr, "baud", d);
    double norad = 0;
    int has_norad = jo_num(tx, "norad_cat_id", &norad);
    if (has_norad) cJSON_AddNumberToObject(pr, "norad_cat_id", norad);
    cJSON *alive = cJSON_GetObjectItem(tx, "alive");
    if (cJSON_IsBool(alive))
      cJSON_AddBoolToObject(pr, "alive", cJSON_IsTrue(alive));
    cJSON *viol = cJSON_GetObjectItem(tx, "frequency_violation");
    if (cJSON_IsBool(viol))
      cJSON_AddBoolToObject(pr, "frequency_violation", cJSON_IsTrue(viol));
    cJSON_AddStringToObject(pr, "source", "SatNOGS DB");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[192], summary[256], link[160];
    if (has_norad && has_dl)
      snprintf(title, sizeof title, "NORAD %.0f %s downlink %.4f MHz",
               norad, desc ? desc : "transmitter", dl / 1e6);
    else if (has_norad)
      snprintf(title, sizeof title, "NORAD %.0f %s transmitter",
               norad, desc ? desc : "");
    else
      snprintf(title, sizeof title, "SatNOGS transmitter %s", uuid);
    char uptxt[48] = "";
    if (has_ul) snprintf(uptxt, sizeof uptxt, " · uplink %.4f MHz", ul / 1e6);
    snprintf(summary, sizeof summary, "%s%s%s%s",
             mode ? mode : "mode n/a",
             stat ? " · " : "", stat ? stat : "", uptxt);
    snprintf(link, sizeof link, "https://db.satnogs.org/satellite/%s",
             satid ? satid : "");

    intel_item it = {0};
    it.remote_key      = uuid;
    it.title           = title;
    it.summary         = summary;
    it.link            = satid ? link : NULL;
    it.lang            = "en";
    it.published_at    = jo_sv(tx, "updated");
    it.record_type     = "satellite-transmitter";
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"satellite\",\"rf\",\"satnogs\"]";
    /* R2: no coordinates in this dataset. */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[satnogs-db-transmitters] emitted %d\n", n);
  return 0;
}

static const source_def tsp_satnogs_transmitters_def = {
  .id = "satnogs-db-transmitters", .collector = "satellite",
  .name = "SatNOGS DB satellite transmitters (downlink frequency registry)",
  .update_interval_sec = 86400, .run = run,
  .category = "satellite", .type = "api", .url = SATNOGS_TX_URL,
  .description = "Per-satellite RF transmitter registry: uplink/downlink frequencies in Hz, modulation mode, baud rate, IARU coordination status, ITU notification and a frequency-violation flag, keyed by NORAD id.",
  .license = "SatNOGS DB (Libre Space Foundation) — CC-BY-SA, keyless API.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_satnogs_transmitters_def)
