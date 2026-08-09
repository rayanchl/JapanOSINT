/* collectors/sources/cyi_atlas_anchors.c
 * RIPE Atlas anchors — the well-known measurement servers of the Atlas mesh.
 * Endpoint: https://atlas.ripe.net/api/v2/anchors/?format=json&page_size=500
 * Keyless; paginated via `next`.
 * Emits per anchor: id, fqdn, ip_v4/as_v4, ip_v6/as_v6, city, country, company,
 * is_disabled, date_live.
 * GEO: `geometry` is REAL upstream GeoJSON (parse_notes: "use it verbatim,
 * never fall back to the city name"). GeoJSON order is [lon, lat]; both values
 * are range-checked before use so a null/garbage coordinate cannot land a row
 * on Null Island (R2).
 * Licence: RIPE NCC Atlas data, CC BY-SA 4.0 style terms; attribute RIPE Atlas.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIRST_URL "https://atlas.ripe.net/api/v2/anchors/?format=json&page_size=500"
#define MAX_PAGES 5

static int run(const source_ctx *ctx, intel_sink *sink) {
  char url[512];
  snprintf(url, sizeof url, "%s", FIRST_URL);

  int n = 0, pages = 0, fetched_any = 0;
  while (url[0] && pages < MAX_PAGES) {
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) {
      if (!fetched_any) { fprintf(stderr, "[atlas-anchors] fetch/parse failed\n"); return -1; }
      break;                                     /* partial page set is fine */
    }
    fetched_any = 1;
    pages++;

    const cJSON *results = cJSON_GetObjectItem(doc, "results");
    const cJSON *a;
    cJSON_ArrayForEach(a, results) {
      const char *fqdn = jo_sv(a, "fqdn");
      if (!fqdn) continue;
      const cJSON *idv = cJSON_GetObjectItem(a, "id");
      char anchor_id[32] = "";
      if (cJSON_IsNumber(idv)) snprintf(anchor_id, sizeof anchor_id, "%.0f", idv->valuedouble);

      cJSON *p = cJSON_CreateObject();
      if (anchor_id[0]) cJSON_AddStringToObject(p, "anchor_id", anchor_id);
      cJSON_AddStringToObject(p, "fqdn", fqdn);
      const char *s;
      if ((s = jo_sv(a, "ip_v4")))   cJSON_AddStringToObject(p, "ip_v4", s);
      if ((s = jo_sv(a, "ip_v6")))   cJSON_AddStringToObject(p, "ip_v6", s);
      if ((s = jo_sv(a, "city")))    cJSON_AddStringToObject(p, "city", s);
      if ((s = jo_sv(a, "country"))) cJSON_AddStringToObject(p, "country", s);
      if ((s = jo_sv(a, "company"))) cJSON_AddStringToObject(p, "company", s);
      const cJSON *v;
      if (cJSON_IsNumber((v = cJSON_GetObjectItem(a, "as_v4"))))
        cJSON_AddNumberToObject(p, "as_v4", v->valuedouble);
      if (cJSON_IsNumber((v = cJSON_GetObjectItem(a, "as_v6"))))
        cJSON_AddNumberToObject(p, "as_v6", v->valuedouble);
      const cJSON *dis = cJSON_GetObjectItem(a, "is_disabled");
      if (cJSON_IsBool(dis)) cJSON_AddBoolToObject(p, "is_disabled", cJSON_IsTrue(dis));
      const char *live = jo_sv(a, "date_live");
      if (live) cJSON_AddStringToObject(p, "date_live", live);
      const char *decom = jo_sv(a, "date_decommissioned");
      if (decom) cJSON_AddStringToObject(p, "date_decommissioned", decom);

      intel_item it = {0};
      /* geometry: {"type":"Point","coordinates":[lon,lat]} — upstream values */
      const cJSON *geom = cJSON_GetObjectItem(a, "geometry");
      if (cJSON_IsObject(geom)) {
        const cJSON *co = cJSON_GetObjectItem(geom, "coordinates");
        if (cJSON_IsArray(co) && cJSON_GetArraySize(co) >= 2) {
          const cJSON *lonj = cJSON_GetArrayItem(co, 0);
          const cJSON *latj = cJSON_GetArrayItem(co, 1);
          if (cJSON_IsNumber(lonj) && cJSON_IsNumber(latj)) {
            double lon = lonj->valuedouble, lat = latj->valuedouble;
            if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
                !(lat == 0.0 && lon == 0.0)) {
              it.has_geo = 1; it.lat = lat; it.lon = lon;
            }
          }
        }
      }
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      const char *city = jo_sv(a, "city"), *cc = jo_sv(a, "country");
      const char *comp = jo_sv(a, "company");
      char summary[384];
      snprintf(summary, sizeof summary, "%s%s%s%s%s",
               city ? city : "", (city && cc) ? ", " : "", cc ? cc : "",
               comp ? " · " : "", comp ? comp : "");
      char link[256];
      snprintf(link, sizeof link, "https://atlas.ripe.net/probes/%s/",
               anchor_id[0] ? anchor_id : "");

      it.remote_key      = anchor_id[0] ? anchor_id : fqdn;
      it.title           = fqdn;
      it.summary         = summary;
      it.link            = link;
      it.lang            = "en";
      it.published_at    = live;
      it.record_type     = "ripe-atlas-anchor";
      it.properties_json = pj;
      it.tags_json       = "[\"cyber\",\"measurement\",\"ripe-atlas\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
    }

    const char *next = jo_sv(doc, "next");
    if (next && strncmp(next, "http", 4) == 0) snprintf(url, sizeof url, "%s", next);
    else url[0] = '\0';
    cJSON_Delete(doc);
  }

  fprintf(stderr, "[atlas-anchors] emitted %d over %d page(s)\n", n, pages);
  return 0;
}

static const source_def cyi_atlas_anchors_def = {
  .id = "atlas-anchors", .collector = "cyber",
  .name = "RIPE Atlas anchors",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = FIRST_URL,
  .description = "The RIPE Atlas anchor mesh with real GeoJSON coordinates, host AS and IPv4/IPv6 addresses — a mappable inventory of measurement vantage points.",
  .license = "RIPE NCC Atlas data (CC BY-SA 4.0 style terms); attribution to RIPE Atlas.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_atlas_anchors_def)
