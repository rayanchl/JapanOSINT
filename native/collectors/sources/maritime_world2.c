/* collectors/sources/maritime_world2.c
 * OSINT service — Datalastic vessel lookup (key-gated), pivot on ctx->entity
 * (a vessel name, IMO, or MMSI). Honest-empty when DATALASTIC_KEY unset.
 *   DATALASTIC_VESSEL  api.datalastic.com/api/v0/vessel_find?name= */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *MR_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int emit_vessel(intel_sink *sink, const cJSON *v) {
  const char *name = jo_sv(v, "name");
  const char *mmsi = jo_sv(v, "mmsi");
  const char *imo = jo_sv(v, "imo");
  const char *type = jo_sv(v, "type");
  const char *country = jo_sv(v, "country_iso");
  const char *uuid = jo_sv(v, "uuid");
  const cJSON *lat = cJSON_GetObjectItem(v, "lat");
  const cJSON *lon = cJSON_GetObjectItem(v, "lon");
  if (!name && !mmsi && !imo) return 0;
  cJSON *d = cJSON_CreateObject();
  if (name) cJSON_AddStringToObject(d, "name", name);
  if (mmsi) cJSON_AddStringToObject(d, "mmsi", mmsi);
  if (imo) cJSON_AddStringToObject(d, "imo", imo);
  if (type) cJSON_AddStringToObject(d, "type", type);
  if (country) cJSON_AddStringToObject(d, "country", country);
  cJSON_AddStringToObject(d, "source", "Datalastic");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "DATALASTIC_VESSEL");
  if (mmsi) cJSON_AddStringToObject(p, "mmsi", mmsi);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  int has_geo = (cJSON_IsNumber(lat) && cJSON_IsNumber(lon));
  char link[256] = {0};
  if (mmsi) snprintf(link, sizeof link, "https://www.marinetraffic.com/en/ais/details/ships/mmsi:%s", mmsi);
  char summ[128]; snprintf(summ, sizeof summ, "%s%s%s", type ? type : "vessel", country ? " · " : "", country ? country : "");
  intel_item it = {0};
  it.remote_key = uuid ? uuid : (mmsi ? mmsi : name);
  it.title = name ? name : (mmsi ? mmsi : imo);
  it.summary = summ; it.body = bj; it.link = link[0] ? link : NULL; it.lang = "en";
  it.has_geo = has_geo; it.lat = has_geo ? lat->valuedouble : 0; it.lon = has_geo ? lon->valuedouble : 0;
  it.record_type = "vessel";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"DATALASTIC_VESSEL\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  if (strcmp(ctx->source_id ? ctx->source_id : "", "DATALASTIC_VESSEL") != 0) return 0;
  const char *k = jo_env("DATALASTIC_KEY");
  if (!k) { fprintf(stderr, "[DATALASTIC_VESSEL] no DATALASTIC_KEY\n"); return 0; }
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://api.datalastic.com/api/v0/vessel_find?api-key=%s&name=%s", k, enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", MR_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "DATALASTIC_VESSEL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  if (cJSON_IsArray(data)) {
    const cJSON *v; cJSON_ArrayForEach(v, data) { emitted += emit_vessel(sink, v); }
  } else if (cJSON_IsObject(data)) {
    /* some endpoints return a single vessel object, or {vessels:[...]} */
    const cJSON *vessels = cJSON_GetObjectItem(data, "vessels");
    if (cJSON_IsArray(vessels)) {
      const cJSON *v; cJSON_ArrayForEach(v, vessels) { emitted += emit_vessel(sink, v); }
    } else {
      emitted += emit_vessel(sink, data);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[DATALASTIC_VESSEL] emitted %d\n", emitted);
  return 0;
}

static const source_def datalastic_def = {
  .id = "DATALASTIC_VESSEL", .collector = "osint",
  .name = "Datalastic Vessel Find", .name_ja = "Datalastic 船舶検索",
  .update_interval_sec = 0, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.datalastic.com",
  .description = "Datalastic (DATALASTIC_KEY): vessel lookup by name/IMO/MMSI (type, flag, position).",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(datalastic_def)
