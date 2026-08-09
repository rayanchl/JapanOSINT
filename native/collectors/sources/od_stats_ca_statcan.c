/* Statistics Canada WDS - cubes changed/released on a date.
 * Endpoint: https://www150.statcan.gc.ca/t1/wds/rest/getChangedCubeList/<date>
 * The date is today (UTC), computed at run time. This is release EVENT data,
 * not observations: the WDS observation endpoints
 * (getDataFromVectorsAndLatestNPeriods, getSeriesInfoFromVector) are POST-only
 * and getChangedSeriesList answers HTTP 409 "product is not released yet", so
 * neither is used here.
 * Emits, per released table: productId, releaseTime and responseStatusCode as
 * returned. A quiet day (no releases) is an honest empty, not an error.
 * Keyless. Licence: Statistics Canada Open Licence - reproduction permitted
 * with attribution. */
#include "od_shared.c"

#define SID "stats-ca-statcan-releases"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char day[16], url[160];
  od_today_utc(day, sizeof day);
  snprintf(url, sizeof url,
           "https://www150.statcan.gc.ca/t1/wds/rest/getChangedCubeList/%s",
           day);

  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *arr = cJSON_GetObjectItem(doc, "object");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    char pidb[48];
    const char *pid = od_scalar(r, "productId", pidb, sizeof pidb);
    if (!pid) continue;
    const char *rel = od_s(r, "releaseTime");

    char title[220];
    snprintf(title, sizeof title, "StatCan table %s released %s", pid,
             rel ? rel : day);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, r);
    cJSON_AddStringToObject(props, "queried_date", day);

    intel_item it = {0};
    it.title = title;
    it.remote_key = pid;
    it.published_at = rel;
    it.link = url;
    it.record_type = "statistics-release";
    it.tags_json = "[\"statistics\",\"ca\",\"release\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_stats_ca_statcan_def = {
  .id = SID, .collector = "statistics",
  .name = "Statistics Canada WDS - cubes released today",
  .update_interval_sec = 21600, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://www150.statcan.gc.ca/t1/wds/rest/getChangedCubeList/",
  .description = "Which Statistics Canada tables were published on a given day, with exact release timestamps - an economic-release tripwire",
  .license = "Statistics Canada Open Licence - reproduction with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_ca_statcan_def)
