/* WHO Global Health Observatory OData.
 * Endpoint: https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=100
 *           (WHOSIS_000001 = life expectancy at birth; the indicator list is
 *           at https://ghoapi.azureedge.net/api/Indicator and $filter=
 *           SpatialDim eq 'JPN' narrows it.)
 * Emits one row per country-year observation: indicator code, SpatialDim
 * (ISO3), ParentLocation (WHO region), TimeDim, the disaggregation Dim1 and
 * NumericValue - the machine-readable figure. Rows whose NumericValue is null
 * are skipped rather than emitted as 0 (Value is only a formatted string and
 * is carried in the properties). Keyless.
 * LICENCE FLAG: WHO GHO data is published under CC BY-NC-SA 3.0 IGO -
 * NON-COMMERCIAL, share-alike. That is stricter than the other sources in
 * this batch and should be checked against the intended platform use. */
#include "od_shared.c"

#define SID "who-gho-indicator"
static const char *URL = "https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=100";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    double num = 0;
    if (!od_numv(r, "NumericValue", &num)) continue;  /* null -> skip, not 0 */
    const char *ind = od_s(r, "IndicatorCode");
    const char *spatial = od_s(r, "SpatialDim");
    char timeb[32];
    const char *when = od_scalar(r, "TimeDim", timeb, sizeof timeb);
    const char *dim1 = od_s(r, "Dim1");

    char title[400];
    snprintf(title, sizeof title, "%s %s %s%s%s = %g", ind ? ind : "WHO GHO",
             spatial ? spatial : "", when ? when : "",
             dim1 ? " " : "", dim1 ? dim1 : "", num);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, r);

    char idb[48];
    const char *rid = od_scalar(r, "Id", idb, sizeof idb);

    intel_item it = {0};
    it.title = title;
    it.summary = od_s(r, "ParentLocation");
    it.remote_key = rid;
    it.published_at = od_s(r, "TimeDimensionValue");
    it.link = URL;
    it.record_type = "health-indicator-observation";
    it.tags_json = "[\"statistics\",\"health\",\"who\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_who_gho_def = {
  .id = SID, .collector = "statistics",
  .name = "WHO Global Health Observatory OData",
  .update_interval_sec = 604800, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=100",
  .description = "WHO GHO country-year health observations (life expectancy at birth) with ISO3 country, WHO region and numeric value",
  .license = "CC BY-NC-SA 3.0 IGO (WHO) - non-commercial, share-alike",
  .free_tier = 1,
};
REGISTER_SOURCE(od_who_gho_def)
