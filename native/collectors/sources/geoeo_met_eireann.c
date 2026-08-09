/* Met Eireann (Ireland) national warning JSON — colour-coded weather and
 * agricultural advisories with CAP identifiers and county region codes.
 *
 * Endpoint (keyless):
 *   https://www.met.ie/Open_Data/json/warning_IRELAND.json
 * Emits: capId, type, severity, certainty, level, issued, onset, expiry,
 *        headline, description, regions[], status.
 * Licence: Met Eireann Open Data, CC BY 4.0 — explicitly published for reuse.
 *
 * parse_notes honoured:
 *  - Bare JSON array at the top level.
 *  - NO COORDINATES: `regions` is an array of EI-prefixed county codes
 *    ("EI06"). Every row is emitted with has_geo=0; mapping a county code to a
 *    centroid would be exactly the invented geometry R2 forbids.
 *  - An EMPTY array is the COMMON case (no warning in force) and is R3 return
 *    0, not a fetch failure.
 *  - capId is the CAP identifier and is used as the dedupe key across polls.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "geoeo_common.inc"

static const char *URL = "https://www.met.ie/Open_Data/json/warning_IRELAND.json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 25000);
  if (!doc) {
    fprintf(stderr, "[met-eireann-warnings] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "warnings");
  if (!cJSON_IsArray(arr)) {
    fprintf(stderr, "[met-eireann-warnings] top level is not an array\n");
    cJSON_Delete(doc);
    return -1;
  }

  static const char *KEYS[] = { "capId", "type", "severity", "certainty",
                                "level", "issued", "onset", "expiry",
                                "headline", "description", "status", "awLevel",
                                NULL };
  int n = 0;
  cJSON *w;
  cJSON_ArrayForEach(w, arr) {
    const char *cid = geoeo_str(w, "capId");
    const char *head = geoeo_str(w, "headline");
    if (!cid && !head) continue;
    const char *level = geoeo_str(w, "level");
    const char *type = geoeo_str(w, "type");
    const char *onset = geoeo_str(w, "onset");

    cJSON *props = cJSON_CreateObject();
    geoeo_copy_all(props, w, KEYS);
    cJSON *regions = cJSON_GetObjectItem(w, "regions");
    if (cJSON_IsArray(regions)) {
      cJSON *rarr = cJSON_AddArrayToObject(props, "regions");
      cJSON *r;
      cJSON_ArrayForEach(r, regions)
        if (cJSON_IsString(r) && r->valuestring[0])
          cJSON_AddItemToArray(rarr, cJSON_CreateString(r->valuestring));
    }
    cJSON_AddStringToObject(props, "geo_note",
                            "EI county region codes only; no coordinate is "
                            "published with the warning");
    cJSON_AddStringToObject(props, "provider", "Met Eireann");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char title[448];
    snprintf(title, sizeof title, "%s%s%s", level ? level : "",
             level ? " — " : "", head ? head : (type ? type : cid));

    intel_item it = {0};
    it.remote_key = cid ? cid : title;
    it.title = title;
    it.summary = geoeo_str(w, "description");
    it.lang = "en";
    it.published_at = onset;
    it.record_type = "weather-warning";
    it.has_geo = 0;                          /* county codes only — R2 */
    it.geometry_geojson = NULL;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"weather\",\"warning\",\"ireland\",\"met-eireann\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[met-eireann-warnings] emitted %d warnings\n", n);
  return 0;
}

static const source_def geoeo_met_eireann_def = {
  .id = "met-eireann-warnings", .collector = "environment",
  .name = "Met Eireann Ireland Weather Warnings",
  .update_interval_sec = 1800, .run = run,
  .category = "environment", .type = "api",
  .url = "https://www.met.ie/Open_Data/json/warning_IRELAND.json",
  .description = "Met Eireann's national warning JSON — colour-coded weather and agricultural advisories for Ireland with CAP identifiers and county region codes.",
  .license = "Met Eireann Open Data, CC BY 4.0.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_met_eireann_def)
