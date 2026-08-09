/* USGS Hazard Notification System (HANS) — every US volcano currently above
 * Normal/Green, with its aviation colour code and alert level.
 *
 * Endpoint (keyless):
 *   https://volcanoes.usgs.gov/hans-public/api/volcano/getElevatedVolcanoes
 * Emits: obs_fullname (issuing observatory), obs_abbr, volcano_name, vnum
 *        (Smithsonian Volcano_Number), notice_type_cd, color_code, alert_level,
 *        sent_utc / sent_unixtime, notice_url.
 * Licence: USGS public domain (US Government work). No restriction.
 *
 * GEOMETRY WARNING (from parse_notes): this endpoint returns NO coordinates at
 * all — only vnum. Every row is therefore emitted with has_geo=0 and a NULL
 * geometry. `vnum` is carried in properties precisely so the join against the
 * gvp-holocene-volcanoes layer (Volcano_Number) can be done downstream where a
 * geo_precision can be recorded honestly; pinning the alert to a guessed
 * position here would be an invented coordinate (R2).
 *
 * Top level is a bare JSON array, and on a quiet week it can legitimately be
 * empty — that is R3 return 0, not -1.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL =
    "https://volcanoes.usgs.gov/hans-public/api/volcano/getElevatedVolcanoes";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 25000);
  if (!doc) {
    fprintf(stderr, "[usgs-volcano-alerts] fetch/parse failed\n");
    return -1;
  }
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[usgs-volcano-alerts] top level is not an array\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "obs_fullname", "obs_abbr", "volcano_name",
                                "vnum", "notice_type_cd", "color_code",
                                "alert_level", "sent_utc", "sent_unixtime",
                                "notice_url", NULL };
  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, doc) {
    const char *vname = geoeo_str(e, "volcano_name");
    if (!vname) continue;
    const char *color = geoeo_str(e, "color_code");
    const char *level = geoeo_str(e, "alert_level");
    const char *obs = geoeo_str(e, "obs_fullname");
    const char *sent = geoeo_str(e, "sent_utc");
    const char *url = geoeo_str(e, "notice_url");

    char vnum[48] = {0};
    double vn = 0;
    if (geoeo_numlax(e, "vnum", &vn)) snprintf(vnum, sizeof vnum, "%lld",
                                               (long long)vn);

    char title[320];
    snprintf(title, sizeof title, "%s — %s / %s", vname,
             level ? level : "alert", color ? color : "colour n/a");

    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s", obs ? obs : "USGS",
             sent ? " · " : "", sent ? sent : "");

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, e, KEYS);
    cJSON_AddStringToObject(props, "geo_note",
                            "HANS returns no coordinates; join vnum to the "
                            "Smithsonian Volcano_Number for a position");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char key[128];
    snprintf(key, sizeof key, "%s|%s", vnum[0] ? vnum : vname,
             sent ? sent : (level ? level : ""));

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary;
    it.link = url;
    it.author = obs;
    it.lang = "en";
    it.published_at = sent;
    it.record_type = "volcano-alert";
    it.has_geo = 0;                 /* no coordinate came back — R2 */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"volcano\",\"alert\",\"usa\",\"usgs\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[usgs-volcano-alerts] emitted %d elevated volcanoes\n", n);
  return 0;
}

static const source_def geoeo_usgs_volc_def = {
  .id = "usgs-volcano-alerts", .collector = "environment",
  .name = "USGS Elevated Volcano Alerts (HANS)",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://volcanoes.usgs.gov/hans-public/api/volcano/getElevatedVolcanoes",
  .description = "Every US volcano currently above Normal/Green, with its aviation colour code and alert level, from the USGS Hazard Notification System.",
  .license = "USGS public domain (US Government work).",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_usgs_volc_def)
