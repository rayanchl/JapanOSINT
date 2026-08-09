/* Fintraffic Digitraffic — national road weather camera index.
 * Endpoint: https://tie.digitraffic.fi/api/weathercam/v1/stations
 * Emits: one intel row per camera site — station id, name, collectionStatus,
 * dataUpdatedTime, the number of presets (views) and how many are actually in
 * collection, plus each in-collection preset's image URL, pinned on the
 * station's own published coordinates. Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0. A Digitraffic-User header
 * identifying the client is requested by the publisher and is sent here.
 *
 * Parse notes: GeoJSON FeatureCollection; coordinates are [lon, lat, elev].
 * presets[] is what maps to an actual image — a station whose presets are all
 * inCollection:false has no live imagery, and that is recorded rather than
 * hidden. Image URLs are built from the preset id returned by the API
 * (weathercam.digitraffic.fi/<presetId>.jpg), which is the documented layout.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const DT_HDRS[] = {
  "Digitraffic-User: japanosint-native", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://tie.digitraffic.fi/api/weathercam/v1/stations", DT_HDRS, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-weathercam-stations] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    char sid[64];
    trn_idfield(f, "id", sid, sizeof sid);
    if (!sid[0]) trn_idfield(props, "id", sid, sizeof sid);
    const char *nm = jo_sv(props, "name");
    if (!sid[0] && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Fintraffic");
    trn_put_str(pr, "station_id", sid);
    trn_put_str(pr, "name", nm);
    trn_put_str(pr, "collection_status", jo_sv(props, "collectionStatus"));
    trn_put_str(pr, "data_updated_time", jo_sv(props, "dataUpdatedTime"));
    trn_put_str(pr, "municipality", jo_sv(props, "municipality"));
    trn_put_str(pr, "province", jo_sv(props, "province"));

    cJSON *presets = cJSON_GetObjectItem(props, "presets");
    cJSON *views = cJSON_CreateArray();
    int live = 0, total = 0;
    cJSON *p;
    cJSON_ArrayForEach(p, presets) {
      total++;
      const char *pid = jo_sv(p, "id");
      int inc = trn_bool(p, "inCollection");
      if (!pid || inc != 1) continue;
      live++;
      char url[160];
      snprintf(url, sizeof url,
               "https://weathercam.digitraffic.fi/%s.jpg", pid);
      cJSON *v = cJSON_CreateObject();
      cJSON_AddStringToObject(v, "preset_id", pid);
      trn_put_str(v, "presentation_name", jo_sv(p, "presentationName"));
      trn_put_str(v, "direction", jo_sv(p, "direction"));
      cJSON_AddStringToObject(v, "image_url", url);
      cJSON_AddItemToArray(views, v);
    }
    cJSON_AddNumberToObject(pr, "preset_count", total);
    cJSON_AddNumberToObject(pr, "presets_in_collection", live);
    cJSON_AddItemToObject(pr, "views", views);        /* ownership transferred */
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256], summary[96];
    if (nm) snprintf(title, sizeof title, "%s", nm);
    else    snprintf(title, sizeof title, "Road camera %s", sid);
    snprintf(summary, sizeof summary, "%d of %d views live", live, total);

    intel_item it = {0};
    it.remote_key      = sid[0] ? sid : nm;
    it.title           = title;
    it.summary         = summary;
    it.lang            = "fi";
    it.published_at    = jo_sv(props, "dataUpdatedTime");
    it.record_type     = "traffic-camera";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"transport\",\"camera\",\"finland\",\"road\"]";
    double la, lo;
    if (trn_geom_first_pos(cJSON_GetObjectItem(f, "geometry"), &la, &lo)) {
      it.has_geo = 1; it.lat = la; it.lon = lo;
    }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-weathercam-stations] emitted %d\n", n);
  return 0;
}

static const source_def trn_digitraffic_weathercam_def = {
  .id = "digitraffic-weathercam-stations", .collector = "transport",
  .name = "Fintraffic Digitraffic — road weather camera index",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://tie.digitraffic.fi/api/weathercam/v1/stations",
  .description = "Every Finnish road camera with real coordinates and its preset (view) list — a national traffic-camera index with reachable imagery URLs.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_digitraffic_weathercam_def)
