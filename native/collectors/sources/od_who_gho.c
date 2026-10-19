/* WHO Global Health Observatory OData.
 * Endpoint: https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1000&$skip=0&$count=true
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
 * this batch and should be checked against the intended platform use.
 *
 * PAGED 2026-10-12. `?$top=100` stored 100 of the 14,112 country-year
 * observations this one indicator holds, weekly, forever — the seventh discard
 * shape (tools/audit_url_limits.py). OData's own paging is `$top`/`$skip`, and
 * `$count=true` makes the service publish `@odata.count`, which
 * lib/pagewalk.c already reads as the total it discloses against. Measured
 * 2026-10-12: $top=1000 is honoured, $skip advances, @odata.count = 14,112, so
 * the whole indicator is 15 pages.
 *
 * NOTE for whoever widens this to more indicators: the count is PER INDICATOR
 * (the path segment), not per service, so the ceiling below is sized for this
 * one. A larger indicator needs its own measurement, not a bigger guess. */
#include "od_shared.inc"
#include "lib/pagewalk.h"

#define SID "who-gho-indicator"
static const char *URL = "https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1000&$skip=0&$count=true";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */
/* 14,112 observations at 1,000 a page = 15; 30 is that with headroom. */
#define WHO_PAGE_MAX 30  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */

static int who_page(const source_ctx *ctx, intel_sink *sink, const char *sid,
                    cJSON *doc, void *ud, int *seen) {
  (void)ctx; (void)sid; (void)ud;
  const cJSON *arr = cJSON_GetObjectItem(doc, "value");
  *seen = cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : 0;
  if (!cJSON_IsArray(arr)) return 0;

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
  return od_rc(SID, pw_walk_max(ctx, sink, SID, URL, pw_fetch_json, who_page,
                                NULL, WHO_PAGE_MAX));
}

static const source_def od_who_gho_def = {
  .id = SID, .collector = "statistics",
  .name = "WHO Global Health Observatory OData",
  .update_interval_sec = 604800, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1000&$skip=0&$count=true",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "WHO GHO country-year health observations (life expectancy at birth) with ISO3 country, WHO region and numeric value",
  .license = "CC BY-NC-SA 3.0 IGO (WHO) - non-commercial, share-alike",
  .free_tier = 1,
};
REGISTER_SOURCE(od_who_gho_def)
