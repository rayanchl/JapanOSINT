/* Argentina Series de Tiempo API (datos.gob.ar).
 * Endpoint: https://apis.datos.gob.ar/series/api/series/
 *           ?ids=143.3_NO_PR_2004_A_21&limit=100&format=json
 * PARSE NOTE: data[] is an array of ARRAYS - [date, value] pairs, not objects
 * - and meta[] parallels the requested ids (meta[0] is the catalogue entry,
 * the later entries carry dataset/field titles and units). Points whose value
 * is null are skipped rather than emitted as 0. Several series can be pulled
 * at once with comma-separated ids, and &sort=desc returns the newest first.
 * Emits one row per point: date, value, series id, frequency, units and the
 * dataset title. Keyless.
 * Licence: datos.gob.ar open data, free reuse. */
#include "od_shared.c"

#define SID "stats-ar-datos-series"
static const char *URL =
  "https://apis.datos.gob.ar/series/api/series/"
  "?ids=143.3_NO_PR_2004_A_21&limit=100&format=json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *data = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(data)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  /* meta[] parallels the ids: find the first entry carrying field/dataset. */
  const char *sid = NULL, *units = NULL, *freq = NULL, *dstitle = NULL;
  const cJSON *meta = cJSON_GetObjectItem(doc, "meta");
  const cJSON *m;
  cJSON_ArrayForEach(m, meta) {
    if (!cJSON_IsObject(m)) continue;
    const cJSON *field = cJSON_GetObjectItem(m, "field");
    if (cJSON_IsObject(field)) {
      if (!sid) sid = od_s(field, "id");
      if (!units) units = od_s(field, "units");
      if (!dstitle) dstitle = od_s(field, "description");
    }
    const cJSON *dset = cJSON_GetObjectItem(m, "dataset");
    if (cJSON_IsObject(dset) && !dstitle) dstitle = od_s(dset, "title");
    if (!freq) freq = od_s(m, "frequency");
  }

  int n = 0;
  const cJSON *pt;
  cJSON_ArrayForEach(pt, data) {
    if (!cJSON_IsArray(pt) || cJSON_GetArraySize(pt) < 2) continue;
    const cJSON *dt = cJSON_GetArrayItem(pt, 0);
    const cJSON *vv = cJSON_GetArrayItem(pt, 1);
    if (!cJSON_IsString(dt) || !dt->valuestring) continue;
    if (!cJSON_IsNumber(vv)) continue;          /* null point -> skip, not 0 */

    char title[400];
    snprintf(title, sizeof title, "%s %s = %g%s%s",
             dstitle ? dstitle : (sid ? sid : "datos.gob.ar series"),
             dt->valuestring, vv->valuedouble, units ? " " : "",
             units ? units : "");

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_put_s(props, "series_id", sid);
    od_put_s(props, "dataset", dstitle);
    od_put_s(props, "units", units);
    od_put_s(props, "frequency", freq);
    cJSON_AddStringToObject(props, "date", dt->valuestring);
    cJSON_AddNumberToObject(props, "value", vv->valuedouble);

    char rk[200];
    snprintf(rk, sizeof rk, "%s|%s", sid ? sid : "ar", dt->valuestring);

    intel_item it = {0};
    it.title = title;
    it.remote_key = rk;
    it.published_at = dt->valuestring;
    it.link = URL;
    it.lang = "es";
    it.record_type = "statistic-observation";
    it.tags_json = "[\"statistics\",\"ar\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_stats_ar_series_def = {
  .id = SID, .collector = "statistics",
  .name = "Argentina Series de Tiempo API (datos.gob.ar)",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://apis.datos.gob.ar/series/api/series/?ids=143.3_NO_PR_2004_A_21&limit=100&format=json",
  .description = "Argentine national time-series observations (macro programming series) as dated numeric points with units and frequency",
  .license = "datos.gob.ar open data - free reuse",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_ar_series_def)
