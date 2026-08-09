/* Fintraffic Digitraffic — live Finnish road traffic announcements.
 * Endpoint: https://tie.digitraffic.fi/api/traffic-message/v1/messages
 *           ?inactiveHours=0&situationType=TRAFFIC_ANNOUNCEMENT
 *           &situationType=ROADWORK&situationType=WEIGHT_RESTRICTION
 *           &situationType=EXEMPTED_TRANSPORT
 * Emits: one intel row per situation — situationId, situationType,
 * trafficAnnouncementType, releaseTime, the announcement title and description
 * (English preferred, otherwise the first published language) and the
 * situation's own geometry. Keyless.
 * Licence: Fintraffic Digitraffic, CC BY 4.0. A Digitraffic-User header
 * identifying the client is requested by the publisher and is sent here.
 *
 * Parse notes: GeoJSON FeatureCollection. announcements[] is per-language
 * (FI/SV/EN) and index 0 is NOT reliably English, so the language code is
 * matched explicitly. Geometry may be a Point or a LineString/Polygon covering
 * the affected stretch — the first published position is used as the row point
 * and the full geometry is carried alongside it.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static const char *const DT_HDRS[] = {
  "Digitraffic-User: japanosint-native", NULL };

#define DT_MSG_URL \
  "https://tie.digitraffic.fi/api/traffic-message/v1/messages?inactiveHours=0" \
  "&situationType=TRAFFIC_ANNOUNCEMENT&situationType=ROADWORK" \
  "&situationType=WEIGHT_RESTRICTION&situationType=EXEMPTED_TRANSPORT"

/* Pick the announcement published in `want`, else the first one. */
static const cJSON *pick_announcement(const cJSON *props, const char *want) {
  const cJSON *arr = cJSON_GetObjectItem(props, "announcements");
  const cJSON *first = NULL, *a;
  cJSON_ArrayForEach(a, arr) {
    if (!first) first = a;
    const cJSON *lang = cJSON_GetObjectItem(a, "language");
    const char *code = cJSON_IsString(lang) ? lang->valuestring
                                            : jo_sv(lang, "code");
    if (code && strcasecmp(code, want) == 0) return a;
  }
  return first;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http, DT_MSG_URL, DT_HDRS, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-traffic-messages] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    const char *sitid = jo_sv(props, "situationId");
    if (!sitid) continue;

    const cJSON *ann = pick_announcement(props, "EN");
    const char *title = ann ? jo_sv(ann, "title") : NULL;
    const cJSON *loc = ann ? cJSON_GetObjectItem(ann, "location") : NULL;
    const cJSON *comment = ann ? cJSON_GetObjectItem(ann, "comment") : NULL;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Fintraffic");
    cJSON_AddStringToObject(pr, "situation_id", sitid);
    trn_put_str(pr, "situation_type", jo_sv(props, "situationType"));
    trn_put_str(pr, "traffic_announcement_type",
                jo_sv(props, "trafficAnnouncementType"));
    trn_put_str(pr, "release_time", jo_sv(props, "releaseTime"));
    trn_put_str(pr, "version_time", jo_sv(props, "versionTime"));
    trn_put_str(pr, "title", title);
    if (cJSON_IsString(comment)) trn_put_str(pr, "comment", comment->valuestring);
    if (loc) {
      trn_put_str(pr, "location_description", jo_sv(loc, "description"));
    }
    char *pj = cJSON_PrintUnformatted(pr);

    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    char *gj = (geom && !cJSON_IsNull(geom)) ? cJSON_PrintUnformatted(geom)
                                             : NULL;
    double la, lo; int geo = trn_geom_first_pos(geom, &la, &lo);

    char tbuf[320];
    if (title) snprintf(tbuf, sizeof tbuf, "%s", title);
    else snprintf(tbuf, sizeof tbuf, "%s %s",
                  jo_sv(props, "situationType") ?
                    jo_sv(props, "situationType") : "Traffic situation",
                  sitid);

    intel_item it = {0};
    it.remote_key      = sitid;
    it.title           = tbuf;
    it.summary         = (loc ? jo_sv(loc, "description") : NULL);
    it.body            = cJSON_IsString(comment) ? comment->valuestring : NULL;
    it.lang            = "en";
    it.published_at    = jo_sv(props, "releaseTime");
    it.record_type     = "road-incident";
    it.properties_json = pj ? pj : "{}";
    it.geometry_geojson = gj;
    it.tags_json       = "[\"transport\",\"road\",\"finland\",\"incident\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(gj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-traffic-messages] emitted %d\n", n);
  return 0;                      /* a quiet road network is an honest 0 (R3) */
}

static const source_def trn_digitraffic_traffic_messages_def = {
  .id = "digitraffic-traffic-messages", .collector = "transport",
  .name = "Fintraffic Digitraffic — live road traffic announcements",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = DT_MSG_URL,
  .description = "Live Finnish road incidents, closures, roadworks and weight restrictions as geolocated features with multilingual announcement text.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_digitraffic_traffic_messages_def)
