/* collectors/sources/attacksurface_world.c
 * OSINT services — key-gated attack-surface / device-intel providers, pivot on
 * ctx->entity (an IP or domain). Each needs its API key; honest-empty when unset.
 * Because these providers' JSON is large and version-specific, we emit ONE
 * summary intel_item carrying the REAL response (compacted, capped) so nothing
 * is fabricated and no field-shape guess can drop data. Shared keyed_summary().
 *
 *   ONYPHE_SUMMARY   www.onyphe.io/api/v2/summary/ip/<ip>   ONYPHE_API_KEY   (Bearer)
 *   BINARYEDGE_IP    api.binaryedge.io/v2/query/ip/<ip>     BINARYEDGE_API_KEY (X-Key)
 *   FULLHUNT_DOMAIN  fullhunt.io/api/v1/domain/<d>/details  FULLHUNT_API_KEY (X-API-KEY)
 *   CRIMINALIP_IP    api.criminalip.io/v1/asset/ip/report   CRIMINALIP_API_KEY (x-api-key) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *AS_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* Fetch a keyed GET and emit one summary item carrying the real (capped) body. */
static int keyed_summary(const source_ctx *ctx, intel_sink *sink, const char *service,
                         const char *keyenv, const char *url, const char *authhdr,
                         const char *rtype, const char *link) {
  const char *hdrs[] = { "Accept: application/json", AS_UA, authhdr, NULL };
  char *body = jo_get(ctx, url, hdrs, service);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  if (!root) { free(body); return 0; }
  /* Compact + cap the real response so the stored body is bounded but faithful. */
  char *compact = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  free(body);
  if (!compact) return 0;
  const size_t CAP = 8000;
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "service", service);
  cJSON_AddStringToObject(d, "entity", ctx->entity);
  if (strlen(compact) > CAP) {
    char *cut = (char *)malloc(CAP + 1);
    if (cut) { memcpy(cut, compact, CAP); cut[CAP] = 0; cJSON_AddStringToObject(d, "response_truncated", cut); free(cut); }
    cJSON_AddBoolToObject(d, "truncated", 1);
  } else {
    cJSON_AddRawToObject(d, "response", compact);
  }
  free(compact);
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", service);
  cJSON_AddStringToObject(p, "entity", ctx->entity);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  char rk[320], title[320];
  snprintf(rk, sizeof rk, "%s:%s", service, ctx->entity);
  snprintf(title, sizeof title, "%s report: %s", service, ctx->entity);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.link = link; it.lang = "en";
  it.record_type = rtype; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[640], auth[200], link[360];
  int n = 0;
  if (!strcmp(sid, "ONYPHE_SUMMARY")) {
    const char *k = jo_env("ONYPHE_API_KEY");
    if (k) {
      snprintf(url, sizeof url, "https://www.onyphe.io/api/v2/summary/ip/%s", enc);
      snprintf(auth, sizeof auth, "Authorization: bearer %s", k);
      snprintf(link, sizeof link, "https://www.onyphe.io/search?q=%s", enc);
      n = keyed_summary(ctx, sink, "ONYPHE_SUMMARY", "ONYPHE_API_KEY", url, auth, "attack-surface", link);
    } else fprintf(stderr, "[ONYPHE_SUMMARY] no ONYPHE_API_KEY\n");
  } else if (!strcmp(sid, "BINARYEDGE_IP")) {
    const char *k = jo_env("BINARYEDGE_API_KEY");
    if (k) {
      snprintf(url, sizeof url, "https://api.binaryedge.io/v2/query/ip/%s", enc);
      snprintf(auth, sizeof auth, "X-Key: %s", k);
      snprintf(link, sizeof link, "https://app.binaryedge.io/services/query?query=%s", enc);
      n = keyed_summary(ctx, sink, "BINARYEDGE_IP", "BINARYEDGE_API_KEY", url, auth, "attack-surface", link);
    } else fprintf(stderr, "[BINARYEDGE_IP] no BINARYEDGE_API_KEY\n");
  } else if (!strcmp(sid, "FULLHUNT_DOMAIN")) {
    const char *k = jo_env("FULLHUNT_API_KEY");
    if (k) {
      snprintf(url, sizeof url, "https://fullhunt.io/api/v1/domain/%s/details", enc);
      snprintf(auth, sizeof auth, "X-API-KEY: %s", k);
      snprintf(link, sizeof link, "https://fullhunt.io/domain/%s", enc);
      n = keyed_summary(ctx, sink, "FULLHUNT_DOMAIN", "FULLHUNT_API_KEY", url, auth, "attack-surface", link);
    } else fprintf(stderr, "[FULLHUNT_DOMAIN] no FULLHUNT_API_KEY\n");
  } else if (!strcmp(sid, "CRIMINALIP_IP")) {
    const char *k = jo_env("CRIMINALIP_API_KEY");
    if (k) {
      snprintf(url, sizeof url, "https://api.criminalip.io/v1/asset/ip/report?ip=%s", enc);
      snprintf(auth, sizeof auth, "x-api-key: %s", k);
      snprintf(link, sizeof link, "https://www.criminalip.io/asset/report/%s", enc);
      n = keyed_summary(ctx, sink, "CRIMINALIP_IP", "CRIMINALIP_API_KEY", url, auth, "attack-surface", link);
    } else fprintf(stderr, "[CRIMINALIP_IP] no CRIMINALIP_API_KEY\n");
  }
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define AS_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/attacksurface", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

AS_DEF(as_onyphe_def, "ONYPHE_SUMMARY", "ONYPHE IP Summary",
  "ONYPHE (ONYPHE_API_KEY): cyber-defense summary for an IP.");
AS_DEF(as_binaryedge_def, "BINARYEDGE_IP", "BinaryEdge IP Query",
  "BinaryEdge (BINARYEDGE_API_KEY): internet-scan data for an IP.");
AS_DEF(as_fullhunt_def, "FULLHUNT_DOMAIN", "FullHunt Domain Details",
  "FullHunt (FULLHUNT_API_KEY): attack-surface details for a domain.");
AS_DEF(as_criminalip_def, "CRIMINALIP_IP", "Criminal IP Report",
  "Criminal IP (CRIMINALIP_API_KEY): asset/threat report for an IP.");
