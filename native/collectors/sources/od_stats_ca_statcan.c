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
#include "od_shared.inc"

#define SID "stats-ca-statcan-releases"

/* One date's release list. Returns records emitted, 0 for an honest empty and
 * -1 for a failed exchange.
 *
 * StatCan answers HTTP 409 {"message":"The product is not released yet"} for a
 * date whose releases have not gone out (they go out at 08:30 ET), and the
 * collector used to fetch through feed_get_json, which turns every non-2xx into
 * NULL — so each run before the day's release was reported as a fetch failure
 * (measured 2026-09-15: today 409, 2026-09-14 200 with the day's tables). A 409
 * is now the not-yet-released empty it is. */
static int statcan_day(const source_ctx *ctx, intel_sink *sink, const char *day) {
  char url[160];
  snprintf(url, sizeof url,
           "https://www150.statcan.gc.ca/t1/wds/rest/getChangedCubeList/%s",
           day);
  http_response resp = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 30000, 2, &resp) != 0) {
    http_response_free(&resp);
    return -1;
  }
  if (resp.status == 409) { http_response_free(&resp); return 0; }
  if (resp.status < 200 || resp.status >= 300 || !resp.body) {
    http_response_free(&resp);
    return -1;
  }
  cJSON *doc = cJSON_Parse(resp.body);
  http_response_free(&resp);
  if (!doc) return -1;
  const cJSON *arr = cJSON_GetObjectItem(doc, "object");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return -1; }

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

    /* A table released on consecutive days is two releases. */
    char key[96];
    snprintf(key, sizeof key, "%s|%s", pid, rel ? rel : day);

    intel_item it = {0};
    it.title = title;
    it.remote_key = key;
    it.published_at = rel;
    it.link = url;
    it.record_type = "statistics-release";
    it.tags_json = "[\"statistics\",\"ca\",\"release\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return n;
}

/* Yesterday and today (UTC): a run before today's 08:30 ET release still
 * carries the most recent completed release day instead of nothing. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  char today[16], yesterday[16];
  od_today_utc(today, sizeof today);
  time_t t = time(NULL) - 86400;
  struct tm g;
  if (gmtime_r(&t, &g)) strftime(yesterday, sizeof yesterday, "%Y-%m-%d", &g);
  else snprintf(yesterday, sizeof yesterday, "%s", today);

  int a = statcan_day(ctx, sink, yesterday);
  int b = strcmp(yesterday, today) ? statcan_day(ctx, sink, today) : 0;
  if (a < 0 && b < 0) return od_rc(SID, -1);
  return od_rc(SID, (a > 0 ? a : 0) + (b > 0 ? b : 0));
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
