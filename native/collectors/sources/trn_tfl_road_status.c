/* Transport for London — road corridor status for the TfL Road Network.
 * Endpoint: https://api.tfl.gov.uk/Road/all/Status
 * Emits: one intel row per corridor — road id, display name, statusSeverity and
 * its description, the aggregation start/end dates, and the corridor's own
 * bounding box (as a GeoJSON Polygon) with the bbox centre as the row point.
 * Keyless.
 * Licence: TfL Open Data — attribution "Powered by TfL Open Data".
 *
 * Parse note (trap): `bounds` and `envelope` are JSON arrays serialised INSIDE
 * a string, e.g. "[[-0.25616,51.5319],[-0.10234,51.6562]]" — they have to be
 * parsed a SECOND time before the numbers are usable. The pairs are
 * [lon,lat] (south-west first, north-east second).
 *
 * R2 note: the resulting point is the centre of a corridor extent the upstream
 * itself published, not an invented location, so it is emitted with
 * geo_precision="corridor-bbox" and the real polygon alongside it.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

/* Parse the double-encoded bounds string into sw/ne corners. */
static int parse_bounds(const char *s, double *lat0, double *lon0,
                        double *lat1, double *lon1) {
  if (!s) return 0;
  cJSON *b = cJSON_Parse(s);                  /* the second parse */
  if (!cJSON_IsArray(b) || cJSON_GetArraySize(b) < 2) {
    cJSON_Delete(b); return 0;
  }
  cJSON *sw = cJSON_GetArrayItem(b, 0), *ne = cJSON_GetArrayItem(b, 1);
  cJSON *x0 = cJSON_GetArrayItem(sw, 0), *y0 = cJSON_GetArrayItem(sw, 1);
  cJSON *x1 = cJSON_GetArrayItem(ne, 0), *y1 = cJSON_GetArrayItem(ne, 1);
  int ok = 0;
  if (cJSON_IsNumber(x0) && cJSON_IsNumber(y0) &&
      cJSON_IsNumber(x1) && cJSON_IsNumber(y1)) {
    *lon0 = x0->valuedouble; *lat0 = y0->valuedouble;
    *lon1 = x1->valuedouble; *lat1 = y1->valuedouble;
    ok = 1;
  }
  cJSON_Delete(b);
  return ok;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://api.tfl.gov.uk/Road/all/Status",
                             20000);
  if (!doc || !cJSON_IsArray(doc)) {
    fprintf(stderr, "[tfl-road-status] fetch/parse failed\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *id = jo_sv(r, "id");
    const char *nm = jo_sv(r, "displayName");
    const char *sev = jo_sv(r, "statusSeverity");
    if (!id && !nm) continue;

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "operator", "Transport for London");
    trn_put_str(pr, "road_id", id);
    trn_put_str(pr, "display_name", nm);
    trn_put_str(pr, "status_severity", sev);
    trn_put_str(pr, "status_severity_description",
                jo_sv(r, "statusSeverityDescription"));
    trn_put_str(pr, "status_aggregation_start",
                jo_sv(r, "statusAggregationStartDate"));
    trn_put_str(pr, "status_aggregation_end",
                jo_sv(r, "statusAggregationEndDate"));

    double lat0, lon0, lat1, lon1;
    char *gj = NULL;
    double clat = 0, clon = 0; int geo = 0;
    if (parse_bounds(jo_sv(r, "bounds"), &lat0, &lon0, &lat1, &lon1)) {
      clat = (lat0 + lat1) / 2.0; clon = (lon0 + lon1) / 2.0;
      if (trn_geo_ok(clat, clon)) {
        geo = 1;
        cJSON_AddStringToObject(pr, "geo_precision", "corridor-bbox");
        char poly[512];
        snprintf(poly, sizeof poly,
          "{\"type\":\"Polygon\",\"coordinates\":[[[%.6f,%.6f],[%.6f,%.6f],"
          "[%.6f,%.6f],[%.6f,%.6f],[%.6f,%.6f]]]}",
          lon0, lat0, lon1, lat0, lon1, lat1, lon0, lat1, lon0, lat0);
        gj = strdup(poly);
      }
    }
    char *pj = cJSON_PrintUnformatted(pr);

    char title[256];
    snprintf(title, sizeof title, "%s — %s", nm ? nm : id,
             sev ? sev : "status unknown");

    intel_item it = {0};
    it.remote_key      = id ? id : nm;
    it.title           = title;
    it.summary         = jo_sv(r, "statusSeverityDescription");
    it.lang            = "en";
    it.link            = "https://tfl.gov.uk/traffic/status/";
    it.record_type     = "road-corridor-status";
    it.properties_json = pj ? pj : "{}";
    it.geometry_geojson = gj;
    it.tags_json       = "[\"transport\",\"road\",\"london\",\"congestion\"]";
    if (geo) { it.has_geo = 1; it.lat = clat; it.lon = clon; }
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(gj);
    cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[tfl-road-status] emitted %d\n", n);
  return 0;
}

static const source_def trn_tfl_road_status_def = {
  .id = "tfl-road-status", .collector = "transport",
  .name = "TfL road corridor status — London strategic road network",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.tfl.gov.uk/Road/all/Status",
  .description = "Congestion and closure severity for every corridor on the TfL Road Network (A-roads and the ring roads), each with its published bounding box.",
  .license = "TfL Open Data — attribution 'Powered by TfL Open Data'.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_tfl_road_status_def)
