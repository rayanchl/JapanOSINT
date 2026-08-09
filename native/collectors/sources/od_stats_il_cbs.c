/* Israel Central Bureau of Statistics series API.
 * Endpoint: https://apis.cbs.gov.il/series/data/list
 *           ?id=120010&format=json&download=false
 * Emits one row per observation from DataSet.Series[].obs[]: the series id and
 * its path.level1..level4 hierarchy labels, the series update stamp, the
 * TimePeriod and the numeric Value. Observations without a numeric Value are
 * skipped rather than emitted as 0. Labels come back in Hebrew (UTF-8);
 * &lang=en gives English where supported, and series ids are discoverable via
 * the /series/catalog endpoints on the same host. Keyless.
 * Licence: CBS Israel public statistics; the API is openly documented and
 * needs no auth. */
#include "od_shared.c"

#define SID "stats-il-cbs-series"
static const char *URL =
  "https://apis.cbs.gov.il/series/data/list?id=120010&format=json&download=false";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *dataset = cJSON_GetObjectItem(doc, "DataSet");
  const cJSON *series = cJSON_IsObject(dataset)
                          ? cJSON_GetObjectItem(dataset, "Series") : NULL;
  if (!cJSON_IsArray(series)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *s;
  cJSON_ArrayForEach(s, series) {
    if (!cJSON_IsObject(s)) continue;
    char sidb[48];
    const char *sid = od_scalar(s, "id", sidb, sizeof sidb);
    const char *upd = od_s(s, "update");
    const char *name = od_s(s, "name");
    const cJSON *path = cJSON_GetObjectItem(s, "path");
    const char *lvl = NULL;
    if (cJSON_IsObject(path)) {
      static const char *L[] = { "level4", "level3", "level2", "level1", NULL };
      for (int i = 0; L[i] && !lvl; i++) lvl = od_s(path, L[i]);
    }
    const cJSON *obs = cJSON_GetObjectItem(s, "obs");
    const cJSON *o;
    cJSON_ArrayForEach(o, obs) {
      double val = 0;
      if (!od_numv(o, "Value", &val)) continue;   /* no value -> no row */
      const char *when = od_s(o, "TimePeriod");

      char title[420];
      snprintf(title, sizeof title, "CBS IL %s %s %s = %g",
               sid ? sid : "series", lvl ? lvl : (name ? name : ""),
               when ? when : "", val);

      cJSON *props = cJSON_CreateObject();
      if (!props) continue;
      od_put_s(props, "series_id", sid);
      od_put_s(props, "series_name", name);
      od_put_s(props, "path_label", lvl);
      od_put_s(props, "series_update", upd);
      od_put_s(props, "period", when);
      cJSON_AddNumberToObject(props, "value", val);
      if (cJSON_IsObject(path)) {
        static const char *L2[] = { "level1", "level2", "level3", "level4",
                                    NULL };
        od_copy_keys(props, path, L2);
      }

      char rk[160];
      snprintf(rk, sizeof rk, "%s|%s", sid ? sid : "il", when ? when : "");

      intel_item it = {0};
      it.title = title;
      it.remote_key = rk;
      it.published_at = upd;
      it.link = URL;
      it.record_type = "statistic-observation";
      it.tags_json = "[\"statistics\",\"il\"]";
      n += od_emit(sink, &it, props);
    }
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_stats_il_cbs_def = {
  .id = SID, .collector = "statistics",
  .name = "Israel Central Bureau of Statistics series API",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://apis.cbs.gov.il/series/data/list?id=120010&format=json&download=false",
  .description = "Israeli CBS series observations (industrial production index) with hierarchy labels and dated numeric values",
  .license = "CBS Israel public statistics; openly documented API",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_il_cbs_def)
