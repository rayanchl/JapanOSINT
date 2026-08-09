/* Singapore — live HDB carpark availability.
 * Endpoint: https://api.data.gov.sg/v1/transport/carpark-availability
 * Emits: one intel row per carpark — carpark_number, the per-lot-type free and
 * total lot counts (C cars, H heavy vehicles, Y motorcycles), the carpark's own
 * update_datetime and the feed timestamp. Keyless.
 * Licence: data.gov.sg / HDB — Singapore Open Data Licence v1.0.
 *
 * Parse notes: the payload nests as items[0].carpark_data[].carpark_info[].
 * lots_available and total_lots are STRINGS and are coerced. Object key order
 * varies between records, so every field is read by key, never by position.
 * There are NO coordinates in this feed — carpark_number joins to the separate
 * HDB Carpark Information dataset for geography, and no position is invented
 * here (R2).
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define SG_CARPARK_URL "https://api.data.gov.sg/v1/transport/carpark-availability"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SG_CARPARK_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[singapore-carpark-availability] fetch/parse failed\n");
    return -1;
  }
  cJSON *item = cJSON_GetArrayItem(cJSON_GetObjectItem(doc, "items"), 0);
  if (!item) {
    cJSON_Delete(doc);
    fprintf(stderr, "[singapore-carpark-availability] no items returned\n");
    return 0;
  }
  const char *feed_ts = jo_sv(item, "timestamp");

  int n = 0;
  cJSON *cp;
  cJSON_ArrayForEach(cp, cJSON_GetObjectItem(item, "carpark_data")) {
    const char *num = jo_sv(cp, "carpark_number");
    if (!num) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "HDB Singapore (data.gov.sg)");
    cJSON_AddStringToObject(pr, "carpark_number", num);
    trn_put_str(pr, "update_datetime", jo_sv(cp, "update_datetime"));
    trn_put_str(pr, "feed_timestamp", feed_ts);

    cJSON *lots = cJSON_CreateArray();
    double free_total = 0, cap_total = 0;
    cJSON *li;
    cJSON_ArrayForEach(li, cJSON_GetObjectItem(cp, "carpark_info")) {
      double avail, total;
      int ha = trn_num(li, "lots_available", &avail);
      int ht = trn_num(li, "total_lots", &total);
      cJSON *o = cJSON_CreateObject();
      trn_put_str(o, "lot_type", jo_sv(li, "lot_type"));
      if (ha) { cJSON_AddNumberToObject(o, "lots_available", avail);
                free_total += avail; }
      if (ht) { cJSON_AddNumberToObject(o, "total_lots", total);
                cap_total += total; }
      cJSON_AddItemToArray(lots, o);
    }
    cJSON_AddItemToObject(pr, "lots", lots);        /* ownership transferred */
    char *pj = cJSON_PrintUnformatted(pr);

    char title[96], summary[128];
    snprintf(title, sizeof title, "Carpark %s", num);
    if (cap_total > 0)
      snprintf(summary, sizeof summary, "%.0f of %.0f lots free",
               free_total, cap_total);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = num;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.published_at    = jo_sv(cp, "update_datetime");
    it.record_type     = "carpark-availability";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"parking\",\"singapore\",\"live\"]";
    /* no coordinates in this feed (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[singapore-carpark-availability] emitted %d\n", n);
  return 0;
}

static const source_def trn_singapore_carpark_availability_def = {
  .id = "singapore-carpark-availability", .collector = "transport",
  .name = "Singapore — live HDB carpark availability",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = SG_CARPARK_URL,
  .description = "Live free-lot counts for around 2,000 public carparks across Singapore, refreshed each minute and split by lot type.",
  .license = "Singapore Open Data Licence v1.0 (data.gov.sg / HDB).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_singapore_carpark_availability_def)
