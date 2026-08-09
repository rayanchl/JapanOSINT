/* INE Portugal keyless indicator API.
 * Endpoint: https://www.ine.pt/ine/json_indicador/pindica.jsp
 *           ?op=2&varcd=0008273&Dim1=S7A2023&lang=PT   (op=2 data, op=1 meta)
 * PARSE NOTE: the top level is an ARRAY holding one indicator object, and
 * Dados is an OBJECT keyed by period string whose values are arrays of
 * per-dimension rows carrying "valor". Rows without a numeric valor are
 * skipped rather than emitted as 0. lang=EN is available.
 * Emits one row per dimension row: indicator code and description, period,
 * territory (geocod/geodsg), dimension labels and the value. Keyless.
 * Licence: INE Portugal permits reuse with attribution; the API is publicly
 * documented. */
#include "od_shared.c"

#define SID "stats-pt-ine-indicator"
static const char *URL =
  "https://www.ine.pt/ine/json_indicador/pindica.jsp"
  "?op=2&varcd=0008273&Dim1=S7A2023&lang=PT";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *ind;
  cJSON_ArrayForEach(ind, doc) {
    const char *cod = od_s(ind, "IndicadorCod");
    const char *dsg = od_s(ind, "IndicadorDsg");
    const char *upd = od_s(ind, "DataUltimoAtualizacao");
    const cJSON *dados = cJSON_GetObjectItem(ind, "Dados");
    if (!cJSON_IsObject(dados)) continue;

    const cJSON *per;
    cJSON_ArrayForEach(per, dados) {
      if (!per->string || !cJSON_IsArray(per)) continue;
      const cJSON *row;
      cJSON_ArrayForEach(row, per) {
        double valor = 0;
        if (!od_numv(row, "valor", &valor)) continue;   /* no value -> no row */
        const char *geo = od_s(row, "geodsg");
        const char *d3 = od_s(row, "dim_3_t");

        char title[480];
        snprintf(title, sizeof title, "%s - %s%s%s %s = %g",
                 dsg ? dsg : (cod ? cod : "INE"), geo ? geo : "",
                 d3 ? " / " : "", d3 ? d3 : "", per->string, valor);

        cJSON *props = cJSON_CreateObject();
        if (!props) continue;
        od_copy_scalars(props, row);
        od_put_s(props, "indicator_code", cod);
        od_put_s(props, "indicator_name", dsg);
        cJSON_AddStringToObject(props, "period", per->string);
        cJSON_AddNumberToObject(props, "value", valor);

        char rk[240];
        snprintf(rk, sizeof rk, "%s|%s|%s", cod ? cod : "ine", per->string,
                 od_s(row, "geocod") ? od_s(row, "geocod") : (geo ? geo : ""));

        intel_item it = {0};
        it.title = title;
        it.remote_key = rk;
        it.published_at = upd;
        it.link = URL;
        it.lang = "pt";
        it.record_type = "statistic-observation";
        it.tags_json = "[\"statistics\",\"pt\"]";
        n += od_emit(sink, &it, props);
      }
    }
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_stats_pt_ine_def = {
  .id = SID, .collector = "statistics",
  .name = "INE Portugal indicator API",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://www.ine.pt/ine/json_indicador/pindica.jsp?op=2&varcd=0008273&Dim1=S7A2023&lang=PT",
  .description = "Statistics Portugal indicator observations (resident population by NUTS region, sex and age group) with per-dimension breakdowns",
  .license = "INE Portugal - reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_pt_ine_def)
