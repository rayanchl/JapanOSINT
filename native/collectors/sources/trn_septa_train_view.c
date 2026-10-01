/* SEPTA Philadelphia — live Regional Rail train positions.
 * Endpoint: https://www3.septa.org/api/TrainView/index.php
 * Emits: one intel row per train — train number, line, destination, current and
 * next stop, minutes late, track assignment, service type and the railcar
 * consist, pinned on the train's own live lat/lon. Keyless.
 * Licence: SEPTA open third-party API, keyless and documented for public use.
 *
 * Parse notes: top-level JSON array. lat/lon/heading come back as STRINGS while
 * `late` is a number — trn_num coerces both forms, so no field is read at the
 * wrong type. `consist` is a comma-joined list of railcar numbers.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define SEPTA_URL "https://www3.septa.org/api/TrainView/index.php"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, SEPTA_URL, 20000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[septa-train-view] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *t;
  cJSON_ArrayForEach(t, doc) {
    char num[32];
    trn_idfield(t, "trainno", num, sizeof num);
    if (!num[0]) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "SEPTA");
    cJSON_AddStringToObject(pr, "train_number", num);
    trn_put_str(pr, "line", jo_sv(t, "line"));
    trn_put_str(pr, "destination", jo_sv(t, "dest"));
    trn_put_str(pr, "current_stop", jo_sv(t, "currentstop"));
    trn_put_str(pr, "next_stop", jo_sv(t, "nextstop"));
    trn_put_str(pr, "service", jo_sv(t, "service"));
    trn_put_str(pr, "consist", jo_sv(t, "consist"));
    trn_put_str(pr, "track", jo_sv(t, "TRACK"));
    trn_put_str(pr, "track_change", jo_sv(t, "TRACK_CHANGE"));
    trn_put_num(pr, "late_minutes", t, "late");
    trn_put_num(pr, "heading", t, "heading");
    trn_put_str(pr, "source_station", jo_sv(t, "SOURCE"));
    cJSON_AddStringToObject(pr, "geo_precision", "vehicle-gps");
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256], summary[192];
    const char *line = jo_sv(t, "line"), *dest = jo_sv(t, "dest");
    if (line && dest)
      snprintf(title, sizeof title, "SEPTA %s to %s (%s line)", num, dest, line);
    else snprintf(title, sizeof title, "SEPTA train %s", num);
    double late;
    const char *nxt = jo_sv(t, "nextstop");
    if (trn_num(t, "late", &late) && nxt)
      snprintf(summary, sizeof summary, "next %s · %.0f min late", nxt, late);
    else if (nxt) snprintf(summary, sizeof summary, "next %s", nxt);
    else summary[0] = 0;

    intel_item it = {0};
    it.remote_key      = num;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.lang            = "en";
    it.record_type     = "train-position";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"rail\",\"philadelphia\",\"live\"]";
    double la, lo;
    if (trn_num(t, "lat", &la) && trn_num(t, "lon", &lo) && trn_geo_ok(la, lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[septa-train-view] emitted %d\n", n);
  return 0;
}

static const source_def trn_septa_train_view_def = {
  .id = "septa-train-view", .collector = "transport",
  .name = "SEPTA Philadelphia — live Regional Rail train positions",
  .update_interval_sec = 60, .run = run,
  .category = "transport", .type = "api",
  .url = SEPTA_URL,
  .description = "Live position, heading, lateness, track assignment and railcar consist for every SEPTA Regional Rail train in the Philadelphia area.",
  .license = "SEPTA open third-party API, keyless and documented for public use.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_septa_train_view_def)
