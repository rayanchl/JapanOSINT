/* collectors/sources/tsp_ripe_atlas_anchors.c
 * RIPE Atlas anchors — the ~1,800 well-known, operator-hosted measurement
 * endpoints, which doubles as a directory of which telco hosts network
 * measurement infrastructure where.
 * Endpoint: https://atlas.ripe.net/api/v2/anchors/?format=json&page_size=500&page=N
 *   (keyless)
 * Emits: anchor id, type, hostname, FQDN, probe id, IPv4/IPv6 addresses and
 *   their ASNs, IPv4-only flag, city, country, hosting company, NIC handle,
 *   hardware version, is_disabled, date_live, date_decommissioned.
 *
 * parse_notes honoured, quoted:
 *  - "Envelope {count,next,previous,results}; page with page_size (max 500) +
 *    page".
 *  - "Geo is GeoJSON geometry.coordinates = [longitude, latitude] — LON FIRST,
 *    easy to swap by mistake": index 0 is taken as longitude and index 1 as
 *    latitude, and both are range-checked, so a swapped pair cannot slip
 *    through as a plausible-looking point (R2).
 *  - "Filter is_disabled and date_decommissioned to get the live set": neither
 *    is used to drop rows (a decommissioned anchor is real history) — both are
 *    emitted, plus a derived `live` boolean, so the live set is one filter away
 *    and nothing dead is presented as live.
 *  - Distinct from the existing atlas_jp.c, which reads /api/v2/probes/ for
 *    Japan only.
 * Licence: RIPE NCC Atlas data is published under open/CC0 terms; keyless read
 *   API. Paging is deliberately polite.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_ANCHORS "https://atlas.ripe.net/api/v2/anchors/?format=json&page_size=500"
#define PAGES 5

static int emit_page(cJSON *res, intel_sink *sink) {
  int n = 0;
  cJSON *a;
  cJSON_ArrayForEach(a, res) {
    double aid;
    if (!jo_num(a, "id", &aid)) continue;
    const char *host = jo_sv(a, "hostname");
    const char *fqdn = jo_sv(a, "fqdn");
    if (!host && !fqdn) continue;                 /* no identity -> no row */

    const char *city = jo_sv(a, "city");
    const char *ctry = jo_sv(a, "country");
    const char *comp = jo_sv(a, "company");
    const char *decom = jo_sv(a, "date_decommissioned");
    const cJSON *dis = cJSON_GetObjectItem(a, "is_disabled");
    int disabled = cJSON_IsTrue(dis);

    /* geometry.coordinates is [lon, lat] — LON FIRST */
    double lat = 0, lon = 0;
    int has_geo = 0;
    const cJSON *g = cJSON_GetObjectItem(a, "geometry");
    if (cJSON_IsObject(g)) {
      const cJSON *c = cJSON_GetObjectItem(g, "coordinates");
      if (cJSON_IsArray(c) && cJSON_GetArraySize(c) >= 2) {
        const cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
        if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
          double lo = x->valuedouble, la = y->valuedouble;
          if (la >= -90.0 && la <= 90.0 && lo >= -180.0 && lo <= 180.0 &&
              !(la == 0.0 && lo == 0.0)) {
            lat = la; lon = lo; has_geo = 1;
          }
        }
      }
    }

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "anchor_id", aid);
    jo_add_str(pr, "hostname", host);
    jo_add_str(pr, "fqdn", fqdn);
    jo_add_str(pr, "type", jo_sv(a, "type"));
    jo_add_str(pr, "city", city);
    jo_add_str(pr, "country", ctry);
    jo_add_str(pr, "company", comp);
    jo_add_str(pr, "nic_handle", jo_sv(a, "nic_handle"));
    jo_add_str(pr, "ip_v4", jo_sv(a, "ip_v4"));
    jo_add_str(pr, "ip_v6", jo_sv(a, "ip_v6"));
    jo_add_str(pr, "hardware_version", jo_sv(a, "hardware_version"));
    jo_add_str(pr, "date_live", jo_sv(a, "date_live"));
    jo_add_str(pr, "date_decommissioned", decom);
    double d;
    if (jo_num(a, "probe", &d))  cJSON_AddNumberToObject(pr, "probe_id", d);
    if (jo_num(a, "as_v4", &d))  cJSON_AddNumberToObject(pr, "as_v4", d);
    if (jo_num(a, "as_v6", &d))  cJSON_AddNumberToObject(pr, "as_v6", d);
    const cJSON *v4only = cJSON_GetObjectItem(a, "is_ipv4_only");
    if (cJSON_IsBool(v4only))
      cJSON_AddBoolToObject(pr, "is_ipv4_only", cJSON_IsTrue(v4only));
    cJSON_AddBoolToObject(pr, "is_disabled", disabled ? 1 : 0);
    cJSON_AddBoolToObject(pr, "live", (!disabled && !decom) ? 1 : 0);
    if (has_geo) cJSON_AddStringToObject(pr, "geo_precision", "anchor host site as published by RIPE");
    cJSON_AddStringToObject(pr, "source", "RIPE Atlas anchors API");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[288], summary[288], link[128], key[32];
    snprintf(key, sizeof key, "%.0f", aid);
    snprintf(title, sizeof title, "RIPE Atlas anchor %s%s%s%s%s",
             host ? host : fqdn,
             city ? " — " : "", city ? city : "",
             ctry ? ", " : "", ctry ? ctry : "");
    snprintf(summary, sizeof summary, "%s%s%s",
             comp ? comp : "host n/a",
             disabled ? " · disabled" : (decom ? " · decommissioned " : " · live"),
             decom ? decom : "");
    snprintf(link, sizeof link, "https://atlas.ripe.net/anchors/%.0f/", aid);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = jo_sv(a, "date_live");
    it.record_type     = "ripe-atlas-anchor";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"infrastructure\",\"internet\",\"measurement\",\"ripe\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, pages = 0;
  for (int page = 1; page <= PAGES; page++) {
    char url[192];
    snprintf(url, sizeof url, "%s&page=%d", ATLAS_ANCHORS, page);
    cJSON *doc = feed_get_json(ctx->http, url, 45000);
    if (!doc) break;
    cJSON *res = cJSON_GetObjectItem(doc, "results");
    if (!cJSON_IsArray(res)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(res);
    total += emit_page(res, sink);
    int done = !cJSON_IsString(cJSON_GetObjectItem(doc, "next"));
    cJSON_Delete(doc);
    if (got == 0 || done) break;
  }
  if (pages == 0) {
    fprintf(stderr, "[ripe-atlas-anchors] fetch failed\n");
    return -1;
  }
  fprintf(stderr, "[ripe-atlas-anchors] emitted %d over %d page(s)\n", total, pages);
  return 0;
}

static const source_def tsp_ripe_atlas_anchors_def = {
  .id = "ripe-atlas-anchors", .collector = "infrastructure",
  .name = "RIPE Atlas anchors (measurement infrastructure)",
  .update_interval_sec = 86400, .run = run,
  .category = "infrastructure", .type = "api", .url = ATLAS_ANCHORS,
  .description = "The well-known RIPE Atlas anchors: hosting company, ASN, IPv4/IPv6 addresses, city and coordinates, go-live and decommission dates — a directory of which operator hosts measurement infrastructure where.",
  .license = "RIPE NCC Atlas — open/CC0 terms, keyless read API.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_ripe_atlas_anchors_def)
