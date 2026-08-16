/* Argo float positions — the global profiling float array's recent surfacings,
 * from the Ifremer ERDDAP mirror. The backbone of operational ocean state
 * monitoring and a ready-made maritime-domain reference layer.
 *
 * Endpoint (keyless):
 *   https://erddap.ifremer.fr/erddap/tabledap/ArgoFloats.json
 *   ?platform_number,latitude,longitude,time&time%3E=<7 days ago>&distinct()
 * Emits per surfacing: platform_number, time, latitude, longitude.
 * Licence: Argo data are collected and made freely available by the
 *   International Argo Program and the national programmes that contribute to
 *   it (CC0-equivalent). Ifremer ERDDAP is a public mirror.
 *
 * NULL-GEOMETRY LANDMINE (from parse_notes — this is precisely the failure the
 * prior audit found): the ERDDAP payload is COLUMN-ORIENTED
 * ({table:{columnNames,columnTypes,rows:[[...]]}}) and a slice of rows carry
 * JSON null in BOTH the latitude and longitude positions, e.g.
 * ["1901749", null, null, "2026-07-25T15:56:59Z"]. cJSON_IsNumber() correctly
 * rejects null, but any code path that stringifies the element first produces
 * the literal string "null" and passes a not-empty test. Each ordinate is
 * therefore tested with cJSON_IsNumber and the row is emitted with has_geo=0
 * and a NULL geometry when either is null — never substituting 0,0.
 * Rows are POSITIONAL arrays, so every field is indexed by its position in
 * columnNames, not by key. distinct() is what keeps the response to ~274 KB.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "geoeo_common.inc"

static int col_index(cJSON *names, const char *want) {
  int i = 0;
  cJSON *c;
  cJSON_ArrayForEach(c, names) {
    if (cJSON_IsString(c) && strcmp(c->valuestring, want) == 0) return i;
    i++;
  }
  return -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t since = time(NULL) - 7 * 24 * 3600;
  struct tm tmv;
#if defined(_WIN32)
  gmtime_s(&tmv, &since);
#else
  gmtime_r(&since, &tmv);
#endif
  char url[512];
  snprintf(url, sizeof url,
           "https://erddap.ifremer.fr/erddap/tabledap/ArgoFloats.json"
           "?platform_number,latitude,longitude,time"
           "&time%%3E=%04d-%02d-%02dT00:00:00Z&distinct()",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);

  cJSON *doc = feed_get_json(ctx->http, url, 90000);
  if (!doc) {
    fprintf(stderr, "[argo-float-positions] fetch/parse failed\n");
    return -1;
  }
  cJSON *table = cJSON_GetObjectItem(doc, "table");
  cJSON *names = table ? cJSON_GetObjectItem(table, "columnNames") : NULL;
  cJSON *rows = table ? cJSON_GetObjectItem(table, "rows") : NULL;
  if (!cJSON_IsArray(names) || !cJSON_IsArray(rows)) {
    fprintf(stderr, "[argo-float-positions] not an ERDDAP table document\n");
    cJSON_Delete(doc);
    return -1;
  }

  int i_plat = col_index(names, "platform_number");
  int i_lat = col_index(names, "latitude");
  int i_lon = col_index(names, "longitude");
  int i_time = col_index(names, "time");
  if (i_plat < 0 || i_lat < 0 || i_lon < 0 || i_time < 0) {
    fprintf(stderr, "[argo-float-positions] expected columns missing\n");
    cJSON_Delete(doc);
    return -1;
  }

  int n = 0, nulls = 0;
  cJSON *row;
  cJSON_ArrayForEach(row, rows) {
    if (!cJSON_IsArray(row)) continue;
    cJSON *cp = cJSON_GetArrayItem(row, i_plat);
    cJSON *ct = cJSON_GetArrayItem(row, i_time);
    char plat[64];
    if (cJSON_IsString(cp) && cp->valuestring[0])
      snprintf(plat, sizeof plat, "%s", cp->valuestring);
    else if (cJSON_IsNumber(cp))
      snprintf(plat, sizeof plat, "%lld", (long long)cp->valuedouble);
    else
      continue;
    const char *when = cJSON_IsString(ct) ? ct->valuestring : NULL;

    /* Both ordinates must be real JSON numbers. A null on either side means
     * no position for this surfacing. */
    cJSON *cla = cJSON_GetArrayItem(row, i_lat);
    cJSON *clo = cJSON_GetArrayItem(row, i_lon);
    double lat = 0, lon = 0;
    int geo = 0;
    if (cJSON_IsNumber(cla) && cJSON_IsNumber(clo)) {
      lat = cla->valuedouble;
      lon = clo->valuedouble;
      geo = geoeo_ll_ok(lat, lon);
    }
    if (!geo) nulls++;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "platform_number", plat);
    geoeo_add_str(props, "time", when);
    if (geo) { cJSON_AddNumberToObject(props, "latitude", lat);
               cJSON_AddNumberToObject(props, "longitude", lon); }
    else cJSON_AddStringToObject(props, "geo_note",
                                 "upstream returned null latitude/longitude "
                                 "for this surfacing");
    cJSON_AddStringToObject(props, "programme", "International Argo Program");
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char *gj = NULL;
    if (geo) {
      cJSON *g = geoeo_mk_point(lon, lat);
      gj = cJSON_PrintUnformatted(g);
      cJSON_Delete(g);
    }

    char title[160];
    snprintf(title, sizeof title, "Argo float %s", plat);

    char key[160];
    snprintf(key, sizeof key, "%s|%s", plat, when ? when : "");

    intel_item it = {0};
    it.remote_key = key;
    it.title = title;
    it.summary = when;
    it.lang = "en";
    it.published_at = when;
    it.record_type = "argo-float-position";
    it.has_geo = geo;
    it.lat = lat;
    it.lon = lon;
    it.geometry_geojson = gj;          /* NULL when there was no position */
    it.properties_json = pj ? pj : "{}";
    it.tags_json = "[\"ocean\",\"argo\",\"float\",\"maritime\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[argo-float-positions] emitted %d surfacings (%d with a "
                  "null position)\n", n, nulls);
  return 0;
}

static const source_def geoeo_argo_def = {
  .id = "argo-float-positions", .collector = "environment",
  .name = "Argo Float Positions (Ifremer ERDDAP)",
  .update_interval_sec = 21600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://erddap.ifremer.fr/erddap/tabledap/ArgoFloats.json",
  .description = "Live positions of the global Argo profiling float array — recent surfacings across every ocean basin, the backbone of operational ocean state monitoring.",
  .license = "International Argo Program and contributing national programmes, freely available (CC0-equivalent); Ifremer ERDDAP public mirror.",
  .free_tier = 1,
};
REGISTER_SOURCE(geoeo_argo_def)
