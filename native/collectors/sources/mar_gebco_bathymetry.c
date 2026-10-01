/* collectors/sources/mar_gebco_bathymetry.c
 * OSINT service — GEBCO_BATHYMETRY. On-demand coordinate pivot
 * (ctx->entity = "lat,lon"): global seabed depth from the GEBCO 2020 grid via
 * the public OpenTopoData instance — the worldwide equivalent of the EMODnet
 * sampler, usable in Japanese and Indo-Pacific waters.
 *
 * Endpoint: https://api.opentopodata.org/v1/gebco2020?locations=<lat>,<lon>
 * Emits (all fetched): dataset, elevation (metres, negative for seabed) and
 *   the echoed coordinate from results[].location.lat / .lng.
 * Keyless. Licence: the GEBCO grid is freely available; served by the public
 *   OpenTopoData instance, which is rate-limited to 1 call/second and 1,000
 *   calls/day and asks that heavy users self-host. This is an on-demand pivot
 *   (update_interval_sec = 0), so it never polls.
 *
 * parse_notes trap — "Coordinate is echoed in results[].location.lat / .lng
 * (note 'lng'), matching the input order." The emitted point is the echoed
 * one, not the parsed request (R2).
 * parse_notes trap — "elevation is metres, NEGATIVE for seabed; a positive
 * value means the point is on land and should not be emitted as a depth."
 * Positive elevations are skipped.
 * parse_notes — "status:'OK' on success; over-quota returns HTTP 429": any
 * non-200 or non-OK status is an honest empty (return 0).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  double qlat = 0, qlon = 0;
  if (!jo_parse_coord(ctx->entity, &qlat, &qlon)) return 0;

  char url[200];
  snprintf(url, sizeof url,
           "https://api.opentopodata.org/v1/gebco2020?locations=%.6f,%.6f",
           qlat, qlon);

  char *body = jo_get(ctx, url, NULL, "GEBCO_BATHYMETRY");
  if (!body) return 0;                 /* 429 over-quota etc. -> honest empty */
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;

  const char *status = jo_sv(doc, "status");
  if (!status || strcmp(status, "OK") != 0) { cJSON_Delete(doc); return 0; }

  cJSON *res = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, res) {
    const cJSON *elev = cJSON_GetObjectItem(r, "elevation");
    cJSON *loc = cJSON_GetObjectItem(r, "location");
    const cJSON *la = loc ? cJSON_GetObjectItem(loc, "lat") : NULL;
    const cJSON *ln = loc ? cJSON_GetObjectItem(loc, "lng") : NULL;  /* 'lng' */
    if (!cJSON_IsNumber(elev) || !cJSON_IsNumber(la) || !cJSON_IsNumber(ln))
      continue;
    /* a positive elevation is land, not a depth — do not emit it as one */
    if (elev->valuedouble >= 0) continue;

    const char *ds = jo_sv(r, "dataset");
    char key[64], title[160], summary[160];
    snprintf(key, sizeof key, "%.5f,%.5f", la->valuedouble, ln->valuedouble);
    snprintf(title, sizeof title, "GEBCO depth at %.5f, %.5f",
             la->valuedouble, ln->valuedouble);
    snprintf(summary, sizeof summary, "%.1f m (%s)",
             elev->valuedouble, ds ? ds : "gebco2020");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "GEBCO_BATHYMETRY");
    if (ds) cJSON_AddStringToObject(p, "dataset", ds);
    cJSON_AddNumberToObject(p, "elevation_m", elev->valuedouble);
    cJSON_AddNumberToObject(p, "depth_m", -elev->valuedouble);
    cJSON_AddNumberToObject(p, "lat", la->valuedouble);
    cJSON_AddNumberToObject(p, "lon", ln->valuedouble);
    cJSON_AddStringToObject(p, "geo_precision", "grid-cell");
    cJSON_AddStringToObject(p, "source", "GEBCO 2020 via OpenTopoData");
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = url;
    it.lang            = "en";
    it.record_type     = "bathymetry-sample";
    it.has_geo         = 1;
    it.lat             = la->valuedouble;
    it.lon             = ln->valuedouble;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"osint-search\",\"maritime\",\"bathymetry\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[GEBCO_BATHYMETRY] emitted %d\n", n);
  return 0;
}

static const source_def mar_gebco_bathymetry_def = {
  .id = "GEBCO_BATHYMETRY", .collector = "osint",
  .name = "GEBCO 2020 global bathymetry at a coordinate (OpenTopoData)",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.opentopodata.org/v1/gebco2020",
  .description = "Global seabed depth (GEBCO 2020 grid) at a coordinate — worldwide equivalent of the EMODnet sampler, usable anywhere including Japanese and Indo-Pacific waters.",
  .license = "GEBCO grid freely available; served by the public OpenTopoData instance (1 call/s, 1,000 calls/day)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_gebco_bathymetry_def)
