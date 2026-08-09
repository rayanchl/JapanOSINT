/* collectors/sources/cyi_caida_asrank_top.c
 * CAIDA AS Rank — the global transit customer-cone ranking of ASNs.
 * Endpoint: https://api.asrank.caida.org/v2/restful/asns/?first=100  (keyless)
 * parse_notes: "REST wrapper over GraphQL: rows are data.asns.edges[].node".
 * Emits per AS: rank, asn, asnName, country iso, customer-cone ASNs/prefixes/
 * addresses, and the customer/peer/provider degrees — all upstream values.
 * GEO: CAIDA's latitude/longitude is an INFERRED AS centroid, i.e. area level,
 * so it is emitted with geo_precision="as-centroid" and only when the field is
 * a real, in-range number (a blind numeric parse is exactly the bug that stacks
 * rows on Null Island — R2).
 * Licence: CAIDA AS Rank — free with citation of CAIDA AS Rank; review the
 * CAIDA AUP before commercial redistribution of bulk data.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.asrank.caida.org/v2/restful/asns/?first=100"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 25000);
  if (!doc) { fprintf(stderr, "[caida-asrank-top] fetch/parse failed\n"); return -1; }

  const cJSON *edges = cJSON_GetObjectItem(
      cJSON_GetObjectItem(cJSON_GetObjectItem(doc, "data"), "asns"), "edges");

  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, edges) {
    const cJSON *node = cJSON_GetObjectItem(e, "node");
    if (!cJSON_IsObject(node)) continue;
    const char *asn = jo_sv(node, "asn");
    if (!asn) continue;
    const char *name = jo_sv(node, "asnName");
    double rank = 0; int has_rank = jo_numf(node, "rank", &rank);

    const cJSON *country = cJSON_GetObjectItem(node, "country");
    const char *cc = cJSON_IsObject(country) ? jo_sv(country, "iso") : NULL;

    const cJSON *cone = cJSON_GetObjectItem(node, "cone");
    double c_asns = 0, c_pfx = 0, c_addr = 0;
    int hc1 = 0, hc2 = 0, hc3 = 0;
    if (cJSON_IsObject(cone)) {
      hc1 = jo_numf(cone, "numberAsns", &c_asns);
      hc2 = jo_numf(cone, "numberPrefixes", &c_pfx);
      hc3 = jo_numf(cone, "numberAddresses", &c_addr);
    }
    const cJSON *deg = cJSON_GetObjectItem(node, "asnDegree");
    double d_cust = 0, d_peer = 0, d_prov = 0;
    int hd1 = 0, hd2 = 0, hd3 = 0;
    if (cJSON_IsObject(deg)) {
      hd1 = jo_numf(deg, "customer", &d_cust);
      hd2 = jo_numf(deg, "peer", &d_peer);
      hd3 = jo_numf(deg, "provider", &d_prov);
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "asn", asn);
    if (name) cJSON_AddStringToObject(p, "as_name", name);
    if (cc)   cJSON_AddStringToObject(p, "country", cc);
    if (has_rank) cJSON_AddNumberToObject(p, "rank", rank);
    if (hc1) cJSON_AddNumberToObject(p, "cone_asns", c_asns);
    if (hc2) cJSON_AddNumberToObject(p, "cone_prefixes", c_pfx);
    if (hc3) cJSON_AddNumberToObject(p, "cone_addresses", c_addr);
    if (hd1) cJSON_AddNumberToObject(p, "degree_customer", d_cust);
    if (hd2) cJSON_AddNumberToObject(p, "degree_peer", d_peer);
    if (hd3) cJSON_AddNumberToObject(p, "degree_provider", d_prov);

    intel_item it = {0};
    double lat = 0, lon = 0;
    if (jo_numf(node, "latitude", &lat) && jo_numf(node, "longitude", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      it.has_geo = 1; it.lat = lat; it.lon = lon;
      cJSON_AddStringToObject(p, "geo_precision", "as-centroid");
    }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[192];
    snprintf(title, sizeof title, "AS%s %s", asn, name ? name : "");
    char summary[256];
    snprintf(summary, sizeof summary, "rank %.0f · cone %.0f ASNs / %.0f prefixes%s%s",
             rank, c_asns, c_pfx, cc ? " · " : "", cc ? cc : "");
    char link[128];
    snprintf(link, sizeof link, "https://asrank.caida.org/asns/%s", asn);

    it.remote_key      = asn;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "as-rank";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"bgp\",\"asn\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[caida-asrank-top] emitted %d\n", n);
  return 0;
}

static const source_def cyi_caida_asrank_top_def = {
  .id = "caida-asrank-top", .collector = "cyber",
  .name = "CAIDA AS Rank — global AS ranking",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Transit customer-cone ranking of ASNs with degree, country and CAIDA's inferred AS centroid — who actually carries the internet.",
  .license = "CAIDA AS Rank — free with citation; check the CAIDA AUP before bulk commercial redistribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_caida_asrank_top_def)
