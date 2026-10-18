/* WHO Global Health Observatory OData.
 * Endpoint: https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1000&$skip=0
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
#include "od_shared.inc"

#define SID "who-gho-indicator"
/* PAGED. `$top=100` was a discard: measured 2026-10-18, this indicator holds
 * 14,112 country-year observations ($count=true says so) and the collector
 * kept the first 100 of them, forever, with nothing on the run line to say so.
 * GHO publishes no `@odata.nextLink`, so the earlier audit filed it as "no
 * continuation" — but OData's `$skip` IS the continuation, and this service
 * honours it (page 2 shares zero records with page 1). pw_walk only advances a
 * parameter the author wrote, so `$skip=0` is written here deliberately and
 * `$top` is raised to the measured 1,000-row page: 14,112 / 1,000 = 15 pages,
 * inside the 20-page ceiling, so the walk COMPLETES rather than disclosing a
 * sample. 0.6 MB a page, once a week. */
static const char *URL =
  "https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1000&$skip=0";

/* pw_emit_fn. `seen` is what the page CONTAINED, not what was emitted: a null
 * NumericValue is skipped below, and reporting the emitted count would make a
 * full page look short — stopping the walk and suppressing its own truncation
 * notice at the same time (lib/pagewalk.h). */
static int gho_emit_page(const source_ctx *ctx, intel_sink *sink,
                         const char *sid, cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)sid; (void)ud;
  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  if (!cJSON_IsArray(arr)) { *seen = 0; return 0; }
  *seen = cJSON_GetArraySize(arr);

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
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_wrc(SID, pw_walk(ctx, sink, SID, URL, pw_fetch_json,
                             gho_emit_page, NULL));
}

static const source_def od_who_gho_def = {
  .id = SID, .collector = "statistics",
  .name = "WHO Global Health Observatory OData",
  .update_interval_sec = 604800, .run = run,
  .category = "statistics", .type = "api",
  /* The scanner reads a `$skip=` in any URL literal as a fetch site. This one is
   * source_def.url — the endpoint this row documents — and the request is made
   * by run() above, which walks it with pw_walk. Measured 2026-10-18: 14,112
   * observations across 15 pages. */
  .url = "https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1000&$skip=0",  /* exhaustive-ok: metadata field, not a fetch; run() walks it with pw_walk */
  .description = "WHO GHO country-year health observations (life expectancy at birth) with ISO3 country, WHO region and numeric value",
  .license = "CC BY-NC-SA 3.0 IGO (WHO) - non-commercial, share-alike",
  .free_tier = 1,
};
REGISTER_SOURCE(od_who_gho_def)
