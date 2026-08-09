/* collectors/sources/cyi_bgp_ranking_asn.c
 * OSINT service — BGP_RANKING_ASN. ASN pivot giving an AS a single defensible
 * reputation number: how badly it behaves relative to every other AS, computed
 * from the volume of malicious activity seen in its ranges. Nothing else in the
 * fleet provides this.
 * Endpoint: https://bgpranking-ng.circl.lu/json/asn                    (keyless)
 * TRAP (parse_notes): "POST with body {\"asn\":\"13335\"} and Content-Type
 * application/json — a GET returns HTTP 405." So this is a POST, not a GET.
 * "Lower position number = worse; carry total_known_asns so the position is
 * interpretable" — both are carried.
 * Emits ONE row: asn, asn_description, ranking.rank, ranking.position,
 * ranking.total_known_asns. No coordinates -> has_geo 0 (R2).
 * Licence: CIRCL public service, free for research and operational use.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define CYI_URL "https://bgpranking-ng.circl.lu/json/asn"

static int run(const source_ctx *ctx, intel_sink *sink) {
  unsigned long asn = jo_parse_asn(ctx->entity);
  if (!asn) return 0;                              /* wrong shape -> no-op */

  char reqbody[64];
  snprintf(reqbody, sizeof reqbody, "{\"asn\":\"%lu\"}", asn);
  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };

  cJSON *root = feed_post_json(ctx->http, CYI_URL, reqbody, hdrs, 20000);
  if (!root) { fprintf(stderr, "[BGP_RANKING_ASN] POST failed for AS%lu\n", asn); return -1; }

  const cJSON *resp = cJSON_GetObjectItem(root, "response");
  if (!cJSON_IsObject(resp)) {
    cJSON_Delete(root);
    fprintf(stderr, "[BGP_RANKING_ASN] no response object for AS%lu\n", asn);
    return 0;                          /* unranked AS: honest empty (R3) */
  }
  const cJSON *ranking = cJSON_GetObjectItem(resp, "ranking");
  const char *desc = jo_sv(resp, "asn_description");

  double rank = 0, position = 0, total = 0;
  int hr = 0, hp = 0, ht = 0;
  if (cJSON_IsObject(ranking)) {
    hr = jo_numf(ranking, "rank", &rank);
    hp = jo_numf(ranking, "position", &position);
    ht = jo_numf(ranking, "total_known_asns", &total);
  }
  if (!hr && !hp && !desc) {
    cJSON_Delete(root);
    fprintf(stderr, "[BGP_RANKING_ASN] AS%lu not ranked\n", asn);
    return 0;
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "BGP_RANKING_ASN");
  cJSON_AddNumberToObject(p, "asn", (double)asn);
  if (desc) cJSON_AddStringToObject(p, "asn_description", desc);
  if (hr) cJSON_AddNumberToObject(p, "rank", rank);
  if (hp) cJSON_AddNumberToObject(p, "position", position);
  if (ht) cJSON_AddNumberToObject(p, "total_known_asns", total);
  cJSON_AddStringToObject(p, "position_semantics", "lower position = worse behaviour");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[224];
  snprintf(title, sizeof title, "AS%lu %s", asn, desc ? desc : "");
  char summary[256];
  snprintf(summary, sizeof summary, "CIRCL BGP Ranking %.6f · position %.0f of %.0f",
           rank, position, total);

  intel_item it = {0};
  it.remote_key      = title;
  it.title           = title;
  it.summary         = summary;
  it.link            = "https://bgpranking.circl.lu/";
  it.lang            = "en";
  it.record_type     = "as-reputation";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"bgp\",\"reputation\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[BGP_RANKING_ASN] emitted 1 (AS%lu)\n", asn);
  return 0;
}

static const source_def cyi_bgp_ranking_asn_def = {
  .id = "BGP_RANKING_ASN", .collector = "osint",
  .name = "CIRCL BGP Ranking",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "How badly an AS behaves relative to every other AS, computed from the malicious activity seen in its ranges — a single defensible reputation number.",
  .license = "CIRCL public service, free for research and operational use.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_bgp_ranking_asn_def)
