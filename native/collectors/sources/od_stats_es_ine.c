/* INE Spain Tempus3 series data.
 * Endpoint: https://servicios.ine.es/wstempus/js/ES/DATOS_SERIE/IPC251856
 *           ?nult=12   (nult=N returns the last N periods)
 * PARSE NOTE: Data[].Fecha is epoch MILLISECONDS and is converted to an ISO
 * timestamp; Valor is a JSON number and is emitted only when it is actually
 * numeric. Use /EN/ instead of /ES/ for English labels.
 * Emits one row per observation: series code, series name, period date, the
 * value and the Secreto (statistical-secrecy) flag. Keyless.
 * Licence: INE data free for reuse with attribution (Ley 37/2007 / INE reuse
 * terms). */
#include "od_shared.c"

#define SID "stats-es-ine-series"
static const char *URL =
  "https://servicios.ine.es/wstempus/js/ES/DATOS_SERIE/IPC251856?nult=12";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *data = cJSON_GetObjectItem(doc, "Data");
  if (!cJSON_IsArray(data)) { cJSON_Delete(doc); return od_rc(SID, -1); }
  const char *cod = od_s(doc, "COD");
  const char *nombre = od_s(doc, "Nombre");

  int n = 0;
  const cJSON *o;
  cJSON_ArrayForEach(o, data) {
    double valor = 0, fecha = 0;
    if (!od_numv(o, "Valor", &valor)) continue;      /* no value -> no row */
    char iso[40];
    iso[0] = 0;
    if (od_numv(o, "Fecha", &fecha)) od_iso_from_ms(fecha, iso, sizeof iso);

    char title[460];
    snprintf(title, sizeof title, "%s %s = %g", nombre ? nombre : "INE series",
             iso, valor);

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_put_s(props, "series_code", cod);
    od_put_s(props, "series_name", nombre);
    if (iso[0]) cJSON_AddStringToObject(props, "period", iso);
    od_copy(props, o, "Anyo");
    od_copy(props, o, "FK_Periodo");
    od_copy(props, o, "Secreto");
    cJSON_AddNumberToObject(props, "value", valor);

    char rk[200];
    snprintf(rk, sizeof rk, "%s|%s", cod ? cod : "ine", iso);

    intel_item it = {0};
    it.title = title;
    it.remote_key = rk;
    it.published_at = iso[0] ? iso : NULL;
    it.link = URL;
    it.lang = "es";
    it.record_type = "statistic-observation";
    it.tags_json = "[\"statistics\",\"es\",\"inflation\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_stats_es_ine_def = {
  .id = SID, .collector = "statistics",
  .name = "INE Spain Tempus3 series data",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://servicios.ine.es/wstempus/js/ES/DATOS_SERIE/IPC251856?nult=12",
  .description = "Spanish national CPI series observations (dated numeric values) from the keyless INE Tempus3 API",
  .license = "INE Spain - reuse with attribution (Ley 37/2007)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_es_ine_def)
