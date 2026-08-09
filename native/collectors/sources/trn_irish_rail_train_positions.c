/* Irish Rail (Iarnród Éireann) — live train positions.
 * Endpoint: http://api.irishrail.ie/realtime/realtime.asmx/getCurrentTrainsXML
 * Emits: one intel row per running train — TrainCode, TrainStatus, TrainDate,
 * Direction and the public running message, pinned on the TrainLatitude /
 * TrainLongitude the operator publishes for that train. Keyless.
 * Licence: Irish Rail publishes this realtime API openly for third-party use,
 * no registration.
 *
 * Parse notes: SOAP-style XML with a DEFAULT namespace
 * (xmlns=http://api.irishrail.ie/realtime/), so the scan is namespace-agnostic
 * and matches on the local tag name only. The endpoint is HTTP-only — Irish
 * Rail does not offer HTTPS here. PublicMessage embeds literal backslash-n
 * sequences, which are left exactly as published rather than rewritten.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"
#include <stdlib.h>

#define IE_TRAINS_URL \
  "http://api.irishrail.ie/realtime/realtime.asmx/getCurrentTrainsXML"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *xml = feed_get_text(ctx->http, IE_TRAINS_URL, 25000);
  if (!xml) {
    fprintf(stderr, "[irish-rail-train-positions] fetch failed\n");
    return -1;
  }

  int n = 0;
  const char *cur = xml;
  char *rec;
  while ((rec = trn_xml_record(&cur, "objTrainPositions")) != NULL) {
    char *code = trn_xml_text(rec, "TrainCode");
    if (!code || !code[0]) { free(code); free(rec); continue; }

    char *status = trn_xml_text(rec, "TrainStatus");
    char *date   = trn_xml_text(rec, "TrainDate");
    char *dir    = trn_xml_text(rec, "Direction");
    char *msg    = trn_xml_text(rec, "PublicMessage");

    double la, lo;
    int geo = trn_xml_num(rec, "TrainLatitude", &la) &&
              trn_xml_num(rec, "TrainLongitude", &lo) && trn_geo_ok(la, lo);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Iarnrod Eireann");
    cJSON_AddStringToObject(pr, "train_code", code);
    trn_put_str(pr, "train_status", status);
    trn_put_str(pr, "train_date", date);
    trn_put_str(pr, "direction", dir);
    trn_put_str(pr, "public_message", msg);
    if (geo) cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256];
    if (dir && dir[0]) snprintf(title, sizeof title, "Train %s — %s", code, dir);
    else snprintf(title, sizeof title, "Train %s", code);

    intel_item it = {0};
    it.remote_key      = code;
    it.title           = title;
    it.summary         = (msg && msg[0]) ? msg : NULL;
    it.body            = (msg && msg[0]) ? msg : NULL;
    it.lang            = "en";
    it.record_type     = "train-position";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"ireland\",\"live\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); cJSON_Delete(pr);
    free(code); free(status); free(date); free(dir); free(msg); free(rec);
  }
  free(xml);
  fprintf(stderr, "[irish-rail-train-positions] emitted %d\n", n);
  return 0;             /* no trains running overnight is an honest 0 (R3) */
}

static const source_def trn_irish_rail_train_positions_def = {
  .id = "irish-rail-train-positions", .collector = "transport",
  .name = "Irish Rail — live train positions",
  .update_interval_sec = 120, .run = run,
  .category = "transport", .type = "api",
  .url = IE_TRAINS_URL,
  .description = "Live latitude/longitude, direction and public running message for every Iarnrod Eireann train on the island of Ireland, including the Belfast Enterprise.",
  .license = "Irish Rail open realtime API, published for third-party use without registration.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_irish_rail_train_positions_def)
