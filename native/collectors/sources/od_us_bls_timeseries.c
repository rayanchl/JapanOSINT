/* US Bureau of Labor Statistics public API v1 (no registration key).
 * Endpoint: https://api.bls.gov/publicAPI/v1/timeseries/data/
 *           LAUCN040010000000005   (Apache County AZ employment, LAUS)
 * v1 needs no key (v2 does) but is rate-limited to 25 queries/day per IP, so
 * exactly one series is requested and the poll interval is daily. The reply
 * is only trusted when status == "REQUEST_SUCCEEDED"; `value` is a STRING and
 * is parsed as a number, rows that do not parse are skipped.
 * Emits one row per observation: seriesID, year, period/periodName, the value
 * and any footnote text the API attached. Keyless (a descriptive User-Agent
 * is sent, as BLS asks).
 * Licence: US federal government work - public domain. */
#include "od_shared.c"

#define SID "us-bls-timeseries"
static const char *URL =
  "https://api.bls.gov/publicAPI/v1/timeseries/data/LAUCN040010000000005";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (open-source OSINT collector)",
                         NULL };
  cJSON *doc = feed_get_json_h(ctx->http, URL, hdrs, 30000);
  if (!doc) return od_rc(SID, -1);
  const char *status = od_s(doc, "status");
  if (!status || strcmp(status, "REQUEST_SUCCEEDED") != 0) {
    fprintf(stderr, "[%s] upstream status=%s\n", SID, status ? status : "?");
    cJSON_Delete(doc);
    return od_rc(SID, -1);
  }
  const cJSON *results = cJSON_GetObjectItem(doc, "Results");
  const cJSON *series = cJSON_IsObject(results)
                          ? cJSON_GetObjectItem(results, "series") : NULL;
  if (!cJSON_IsArray(series)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *s;
  cJSON_ArrayForEach(s, series) {
    const char *sid = od_s(s, "seriesID");
    const cJSON *data = cJSON_GetObjectItem(s, "data");
    const cJSON *d;
    cJSON_ArrayForEach(d, data) {
      double val = 0;
      if (!od_numv(d, "value", &val)) continue;   /* unparseable -> no row */
      const char *year = od_s(d, "year");
      const char *period = od_s(d, "period");
      const char *pname = od_s(d, "periodName");

      char title[320];
      snprintf(title, sizeof title, "BLS %s %s %s = %g", sid ? sid : "series",
               pname ? pname : (period ? period : ""), year ? year : "", val);

      cJSON *props = cJSON_CreateObject();
      if (!props) continue;
      od_put_s(props, "series_id", sid);
      od_copy_scalars(props, d);
      cJSON_AddNumberToObject(props, "value_num", val);
      const cJSON *fn = cJSON_GetObjectItem(d, "footnotes");
      if (cJSON_IsArray(fn)) {
        const cJSON *f;
        cJSON_ArrayForEach(f, fn) {
          const char *txt = cJSON_IsObject(f) ? od_s(f, "text") : NULL;
          if (txt) { cJSON_AddStringToObject(props, "footnote", txt); break; }
        }
      }

      char rk[160];
      snprintf(rk, sizeof rk, "%s|%s|%s", sid ? sid : "bls",
               year ? year : "", period ? period : "");

      intel_item it = {0};
      it.title = title;
      it.remote_key = rk;
      it.link = URL;
      it.record_type = "statistic-observation";
      it.tags_json = "[\"statistics\",\"us\",\"labour\"]";
      n += od_emit(sink, &it, props);
    }
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_us_bls_def = {
  .id = SID, .collector = "statistics",
  .name = "US Bureau of Labor Statistics public API v1",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://api.bls.gov/publicAPI/v1/timeseries/data/LAUCN040010000000005",
  .description = "County-level US employment observations (LAUS series) as dated numeric values from the keyless BLS v1 API",
  .license = "US federal government work - public domain",
  .free_tier = 1,
};
REGISTER_SOURCE(od_us_bls_def)
