/* collectors/sources/cyi_asrank_asn.c
 * OSINT service — ASRANK_ASN. ASN pivot (ctx->entity = "AS13335"/"13335")
 * answering "how big is this AS really": transit rank, customer cone and the
 * provider/peer/customer degree breakdown. The existing bgpview and RIPEstat
 * lookups report prefixes but not transit position.
 * Endpoint: https://api.asrank.caida.org/v2/restful/asns/<asn>        (keyless)
 * TRAP (parse_notes): "Response is wrapped as data.asn.{...}. A missing AS
 * returns data.asn = NULL with HTTP 200 — check for null and return 0 rows,
 * not -1 (R3)." Handled explicitly below.
 * GEO: "lat/lon are inferred centroids: mark geo_precision or omit" — they are
 * emitted only when they are real, in-range numbers, and always stamped
 * geo_precision="as-centroid" (R2).
 * Licence: CAIDA AS Rank — free with citation; review the CAIDA AUP for
 * commercial use.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  unsigned long asn = jo_parse_asn(ctx->entity);
  if (!asn) return 0;                              /* wrong shape -> no-op */

  char url[160];
  snprintf(url, sizeof url,
           "https://api.asrank.caida.org/v2/restful/asns/%lu", asn);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[ASRANK_ASN] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[ASRANK_ASN] unparseable body\n"); return -1; }

  const cJSON *node = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "data"), "asn");
  if (!cJSON_IsObject(node)) {          /* data.asn == null: AS unknown to CAIDA */
    cJSON_Delete(root);
    fprintf(stderr, "[ASRANK_ASN] AS%lu not in AS Rank\n", asn);
    return 0;                            /* honest empty, NOT an error (R3) */
  }

  const char *name = jo_sv(node, "asnName");
  const cJSON *country = cJSON_GetObjectItem(node, "country");
  const char *cc = cJSON_IsObject(country) ? jo_sv(country, "iso") : NULL;
  double rank = 0; int hrank = jo_numf(node, "rank", &rank);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "ASRANK_ASN");
  cJSON_AddNumberToObject(p, "asn", (double)asn);
  if (name) cJSON_AddStringToObject(p, "as_name", name);
  if (cc)   cJSON_AddStringToObject(p, "country", cc);
  if (hrank) cJSON_AddNumberToObject(p, "rank", rank);
  const cJSON *clique = cJSON_GetObjectItem(node, "cliqueMember");
  if (cJSON_IsBool(clique)) cJSON_AddBoolToObject(p, "clique_member", cJSON_IsTrue(clique));

  double v = 0;
  const cJSON *cone = cJSON_GetObjectItem(node, "cone");
  if (cJSON_IsObject(cone)) {
    if (jo_numf(cone, "numberAsns", &v))      cJSON_AddNumberToObject(p, "cone_asns", v);
    if (jo_numf(cone, "numberPrefixes", &v))  cJSON_AddNumberToObject(p, "cone_prefixes", v);
    if (jo_numf(cone, "numberAddresses", &v)) cJSON_AddNumberToObject(p, "cone_addresses", v);
  }
  const cJSON *deg = cJSON_GetObjectItem(node, "asnDegree");
  if (cJSON_IsObject(deg)) {
    if (jo_numf(deg, "customer", &v)) cJSON_AddNumberToObject(p, "degree_customer", v);
    if (jo_numf(deg, "peer", &v))     cJSON_AddNumberToObject(p, "degree_peer", v);
    if (jo_numf(deg, "provider", &v)) cJSON_AddNumberToObject(p, "degree_provider", v);
    if (jo_numf(deg, "total", &v))    cJSON_AddNumberToObject(p, "degree_total", v);
  }

  intel_item it = {0};
  double lat = 0, lon = 0;
  if (jo_numf(node, "latitude", &lat) && jo_numf(node, "longitude", &lon) &&
      lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
      !(lat == 0.0 && lon == 0.0)) {
    it.has_geo = 1; it.lat = lat; it.lon = lon;
    cJSON_AddStringToObject(p, "geo_precision", "as-centroid");
  }
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  double cone_asns = 0;
  if (cJSON_IsObject(cone)) jo_numf(cone, "numberAsns", &cone_asns);
  char title[192];
  snprintf(title, sizeof title, "AS%lu %s", asn, name ? name : "");
  char summary[288];
  snprintf(summary, sizeof summary,
           "AS Rank %.0f · customer cone %.0f ASNs%s%s",
           rank, cone_asns, cc ? " · " : "", cc ? cc : "");
  char link[128];
  snprintf(link, sizeof link, "https://asrank.caida.org/asns/%lu", asn);

  it.remote_key      = title;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.lang            = "en";
  it.record_type     = "as-rank";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"bgp\",\"asn\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[ASRANK_ASN] emitted 1 (AS%lu)\n", asn);
  return 0;
}

static const source_def cyi_asrank_asn_def = {
  .id = "ASRANK_ASN", .collector = "osint",
  .name = "CAIDA AS Rank — single AS lookup",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://api.asrank.caida.org/v2/restful/asns/",
  .description = "Transit rank, customer cone and provider/peer/customer degree for one AS — the transit position bgpview and RIPEstat do not report.",
  .license = "CAIDA AS Rank — free with citation; review the CAIDA AUP for commercial use.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_asrank_asn_def)
