/* Amtrak live train positions (Amtraker v3).
 * Endpoint: https://api-v3.amtraker.com/v3/trains
 * Emits: one intel row per running train — trainNum, trainID, routeName, live
 * lat/lon, heading, velocity, punctuality text, the origin/destination station
 * codes and the next station with its scheduled and actual times. Keyless.
 * Licence: Amtraker is a community-run API over Amtrak's public track-a-train
 * data, offered free for public use. NOT an Amtrak-official endpoint —
 * attribute Amtraker and keep the poll rate modest (hence the 2-minute cycle).
 *
 * Parse note (trap): the top level is an OBJECT keyed by train number, and each
 * value is an ARRAY of train instances (the same number can be running in more
 * than one place). Iterating it as an array of trains yields nothing.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://api-v3.amtraker.com/v3/trains",
                             30000);
  if (!doc || !cJSON_IsObject(doc)) {
    fprintf(stderr, "[amtrak-live-trains] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *group;
  /* top level: { "1": [ {...}, {...} ], "2": [ ... ], ... } */
  cJSON_ArrayForEach(group, doc) {
    if (!cJSON_IsArray(group)) continue;
    cJSON *t;
    cJSON_ArrayForEach(t, group) {
      if (!cJSON_IsObject(t)) continue;
      char tid[64];
      trn_idfield(t, "trainID", tid, sizeof tid);
      char tnum[32];
      trn_idfield(t, "trainNum", tnum, sizeof tnum);
      if (!tid[0] && !tnum[0]) continue;

      double la, lo;
      int geo = trn_num(t, "lat", &la) && trn_num(t, "lon", &lo) &&
                trn_geo_ok(la, lo);

      /* first station still ahead of the train, if the feed lists one */
      const char *next_name = NULL, *next_code = NULL, *next_sch = NULL;
      const cJSON *st;
      cJSON_ArrayForEach(st, cJSON_GetObjectItem(t, "stations")) {
        const char *status = jo_sv(st, "status");
        if (status && strcasecmp(status, "Departed") == 0) continue;
        next_name = jo_sv(st, "name");
        next_code = jo_sv(st, "code");
        next_sch  = jo_sv(st, "schArr");
        break;
      }

      cJSON *pr = cJSON_CreateObject();
      cJSON_AddStringToObject(pr, "operator", "Amtrak (via Amtraker)");
      trn_put_str(pr, "train_id", tid);
      trn_put_str(pr, "train_number", tnum);
      trn_put_str(pr, "route_name", jo_sv(t, "routeName"));
      trn_put_str(pr, "train_state", jo_sv(t, "trainState"));
      trn_put_str(pr, "punctuality", jo_sv(t, "trainTimely"));
      trn_put_num(pr, "velocity", t, "velocity");
      trn_put_num(pr, "heading_deg", t, "heading");
      trn_put_str(pr, "heading", jo_sv(t, "heading"));
      trn_put_str(pr, "origin_code", jo_sv(t, "origCode"));
      trn_put_str(pr, "destination_code", jo_sv(t, "destCode"));
      trn_put_str(pr, "next_station", next_name);
      trn_put_str(pr, "next_station_code", next_code);
      trn_put_str(pr, "next_station_scheduled", next_sch);
      trn_put_str(pr, "last_val_ts", jo_sv(t, "lastValTS"));
      if (geo) cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
      char *pj = cJSON_PrintUnformatted(pr);

      char title[320], summary[256];
      const char *route = jo_sv(t, "routeName");
      if (route) snprintf(title, sizeof title, "Amtrak %s — %s",
                          tnum[0] ? tnum : tid, route);
      else snprintf(title, sizeof title, "Amtrak train %s",
                    tnum[0] ? tnum : tid);
      const char *timely = jo_sv(t, "trainTimely");
      if (next_name && timely)
        snprintf(summary, sizeof summary, "next %s · %s", next_name, timely);
      else if (timely) snprintf(summary, sizeof summary, "%s", timely);
      else if (next_name) snprintf(summary, sizeof summary, "next %s", next_name);
      else summary[0] = 0;

      intel_item it = {0};
      it.remote_key      = tid[0] ? tid : tnum;
      it.title           = title;
      it.summary         = summary[0] ? summary : NULL;
      it.lang            = "en";
      it.published_at    = jo_sv(t, "lastValTS");
      it.record_type     = "train-position";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"transport\",\"rail\",\"us\",\"live\"]";
      if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
      cJSON_Delete(pr);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[amtrak-live-trains] emitted %d\n", n);
  return 0;
}

static const source_def trn_amtrak_live_trains_def = {
  .id = "amtrak-live-trains", .collector = "transport",
  .name = "Amtrak live train positions (Amtraker v3)",
  .update_interval_sec = 120, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api-v3.amtraker.com/v3/trains",
  .description = "Live position, speed and running status of every Amtrak train in North America, with the next scheduled station per train.",
  .license = "Amtraker community API over Amtrak public track-a-train data; free for public use, attribute Amtraker.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_amtrak_live_trains_def)
