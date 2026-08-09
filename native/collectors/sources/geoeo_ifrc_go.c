/* IFRC GO — the emergency operations register of the International Federation
 * of Red Cross and Red Crescent Societies. The humanitarian-RESPONSE view that
 * GDACS's hazard view does not give you.
 *
 * Endpoint (keyless): https://goadmin.ifrc.org/api/v2/event/?limit=50
 * Emits per emergency: name, disaster type (dtype.name), the affected
 *   countries' iso3 / name / national society, ifrc_severity_level_display,
 *   disaster_start_date, glide number and num_affected.
 * Licence: IFRC GO is an open humanitarian data platform; the v2 API is public
 *   and keyless. IFRC asks for attribution, which is carried on every row.
 *
 * parse_notes honoured:
 *  - Paginated DRF API: {count, next, previous, results[]} — records live under
 *    'results' and 'count' is the corpus total, logged not emitted.
 *  - NO COORDINATES on the event object: countries[] carries iso3/name/
 *    society_name but no lat/lon. Every row is therefore emitted with
 *    has_geo=0. Falling back to a capital city is exactly what R2 forbids;
 *    the iso3 codes are carried so a join to a country layer can be done
 *    downstream with an honest geo_precision.
 *  - num_affected and several severity fields are frequently null and are
 *    dropped rather than stringified.
 *  - 'glide' is often an EMPTY STRING rather than null, so it is only emitted
 *    when non-empty — otherwise it looks like a usable join key to
 *    GDACS/ReliefWeb when it is not.
 *  - dtype is a NESTED OBJECT, not a string.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL = "https://goadmin.ifrc.org/api/v2/event/?limit=50";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 45000);
  if (!doc) {
    fprintf(stderr, "[ifrc-go-emergencies] fetch/parse failed\n");
    return -1;
  }
  cJSON *results = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(results)) {
    fprintf(stderr, "[ifrc-go-emergencies] no results array\n");
    cJSON_Delete(doc);
    return -1;
  }
  double count = 0;
  if (geoeo_num(doc, "count", &count))
    fprintf(stderr, "[ifrc-go-emergencies] register holds %.0f events\n", count);

  static const char *KEYS[] = { "name", "summary", "disaster_start_date",
                                "ifrc_severity_level_display", "num_affected",
                                "num_beneficiaries", "updated_at",
                                "auto_generated_source", "is_featured",
                                "active_deployments", NULL };
  int n = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, results) {
    const char *name = geoeo_str(e, "name");
    double eid = 0;
    int has_id = geoeo_numlax(e, "id", &eid);
    if (!name && !has_id) continue;

    /* dtype is a nested object, not a string. */
    const char *dtype = NULL;
    cJSON *dt = cJSON_GetObjectItem(e, "dtype");
    if (cJSON_IsObject(dt)) dtype = geoeo_str(dt, "name");

    cJSON *props = cJSON_CreateObject();
    if (has_id) cJSON_AddNumberToObject(props, "event_id", eid);
    geoeo_copy_all(props, e, KEYS);
    geoeo_add_str(props, "disaster_type", dtype);
    /* glide is often "" rather than null — only emit a real value. */
    const char *glide = geoeo_str(e, "glide");
    geoeo_add_str(props, "glide", glide);

    const char *ctry0 = NULL;
    cJSON *countries = cJSON_GetObjectItem(e, "countries");
    if (cJSON_IsArray(countries)) {
      cJSON *carr = cJSON_AddArrayToObject(props, "countries");
      cJSON *c;
      cJSON_ArrayForEach(c, countries) {
        cJSON *co = cJSON_CreateObject();
        geoeo_copy(co, c, "iso3");
        geoeo_copy(co, c, "name");
        geoeo_copy(co, c, "society_name");
        cJSON_AddItemToArray(carr, co);
        if (!ctry0) ctry0 = geoeo_str(c, "name");
      }
    }
    cJSON_AddStringToObject(props, "geo_note",
                            "IFRC GO events carry country codes only, no "
                            "coordinate; join iso3 to a country layer rather "
                            "than pinning a capital");
    cJSON_AddStringToObject(props, "attribution", "IFRC GO");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[416];
    snprintf(title, sizeof title, "%s%s%s", name ? name : "IFRC emergency",
             ctry0 ? " — " : "", ctry0 ? ctry0 : "");

    char summary[256];
    const char *sev = geoeo_str(e, "ifrc_severity_level_display");
    const char *start = geoeo_str(e, "disaster_start_date");
    snprintf(summary, sizeof summary, "%s%s%s%s%s", dtype ? dtype : "",
             (dtype && sev) ? " · " : "", sev ? sev : "",
             start ? " · from " : "", start ? start : "");

    char key[64];
    if (has_id) snprintf(key, sizeof key, "%lld", (long long)eid);
    else snprintf(key, sizeof key, "%.60s", name);

    char link[128];
    if (has_id)
      snprintf(link, sizeof link, "https://go.ifrc.org/emergencies/%lld",
               (long long)eid);
    else link[0] = 0;

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = summary[0] ? summary : NULL;
    it.body = geoeo_str(e, "summary");
    it.link = link[0] ? link : NULL;
    it.lang = "en";
    it.published_at = start;
    it.record_type = "humanitarian-emergency";
    it.has_geo = 0;                  /* country codes only — R2 */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"humanitarian\",\"disaster\",\"ifrc\",\"red-cross\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[ifrc-go-emergencies] emitted %d emergencies\n", n);
  return 0;
}

static const source_def geoeo_ifrc_def = {
  .id = "ifrc-go-emergencies", .collector = "environment",
  .name = "IFRC GO Emergency Operations",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://goadmin.ifrc.org/api/v2/event/?limit=50",
  .description = "The IFRC GO platform's emergency register — disaster operations worldwide with type, GLIDE number, severity, affected-population figures and the responding national Red Cross/Red Crescent society.",
  .license = "IFRC GO open humanitarian data platform; public keyless v2 API, attribution requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_ifrc_def)
