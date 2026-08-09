/* IBGE (Brazil) aggregate statistics API - IPCA monthly consumer prices.
 * Endpoint: https://servicodados.ibge.gov.br/api/v3/agregados/1737/periodos/-6/
 *           variaveis/63?localidades=N1%5Ball%5D
 * PARSE NOTE: the square brackets in localidades=N1[all] MUST stay
 * percent-encoded (%5B/%5D) - the raw-bracket form returns nothing. The
 * top level is an ARRAY of variable objects and `serie` is an OBJECT keyed by
 * YYYYMM whose values are STRINGS; IBGE writes "-" / "..." for missing
 * observations, which are skipped rather than parsed as 0.
 * Emits one row per locality/period: variable name, unit, locality, period
 * and the numeric value. Keyless. The same URL shape serves any of the ~1,000
 * IBGE aggregates at national/state/municipal level.
 * Licence: IBGE open data, free reuse with attribution. */
#include "od_shared.c"

#define SID "stats-br-ibge-ipca"
static const char *URL =
  "https://servicodados.ibge.gov.br/api/v3/agregados/1737/periodos/-6/"
  "variaveis/63?localidades=N1%5Ball%5D";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *v;
  cJSON_ArrayForEach(v, doc) {
    const char *variavel = od_s(v, "variavel");
    const char *unidade = od_s(v, "unidade");
    const cJSON *resultados = cJSON_GetObjectItem(v, "resultados");
    const cJSON *res;
    cJSON_ArrayForEach(res, resultados) {
      const cJSON *series = cJSON_GetObjectItem(res, "series");
      const cJSON *s;
      cJSON_ArrayForEach(s, series) {
        const cJSON *loc = cJSON_GetObjectItem(s, "localidade");
        char locidb[48];
        const char *locname = cJSON_IsObject(loc) ? od_s(loc, "nome") : NULL;
        const char *locid = cJSON_IsObject(loc)
                              ? od_scalar(loc, "id", locidb, sizeof locidb)
                              : NULL;
        const cJSON *serie = cJSON_GetObjectItem(s, "serie");
        if (!cJSON_IsObject(serie)) continue;
        const cJSON *p;
        cJSON_ArrayForEach(p, serie) {
          if (!p->string || !cJSON_IsString(p) || !p->valuestring) continue;
          char *end = NULL;
          double val = strtod(p->valuestring, &end);
          if (!end || end == p->valuestring) continue;   /* "-" / "..." */

          char title[420];
          snprintf(title, sizeof title, "%s - %s %s = %s%s%s",
                   variavel ? variavel : "IBGE",
                   locname ? locname : (locid ? locid : ""),
                   p->string, p->valuestring, unidade ? " " : "",
                   unidade ? unidade : "");

          cJSON *props = cJSON_CreateObject();
          if (!props) continue;
          od_put_s(props, "variable", variavel);
          od_put_s(props, "unit", unidade);
          od_put_s(props, "locality", locname);
          od_put_s(props, "locality_id", locid);
          cJSON_AddStringToObject(props, "period", p->string);
          cJSON_AddNumberToObject(props, "value", val);

          char rk[220];
          snprintf(rk, sizeof rk, "%s|%s|%s", locid ? locid : "?",
                   p->string, variavel ? variavel : "v");

          intel_item it = {0};
          it.title = title;
          it.remote_key = rk;
          it.link = URL;
          it.lang = "pt";
          it.record_type = "statistic-observation";
          it.tags_json = "[\"statistics\",\"br\",\"inflation\"]";
          n += od_emit(sink, &it, props);
        }
      }
    }
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_stats_br_ibge_def = {
  .id = SID, .collector = "statistics",
  .name = "IBGE (Brazil) aggregate statistics - IPCA",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://servicodados.ibge.gov.br/api/v3/agregados/1737/periodos/-6/variaveis/63?localidades=N1%5Ball%5D",
  .description = "Brazilian monthly consumer price variation (IPCA) as period/value observations from the IBGE aggregates API",
  .license = "IBGE open data - free reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_br_ibge_def)
