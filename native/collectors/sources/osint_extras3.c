/* collectors/sources/osint_extras3.c
 * OSINT services — additional keyless sources, pivot on ctx->entity. Real fetch
 * or honest-empty. No API keys. (Low-value / variant additions.)
 *
 *   SECURITY_SE_SEARCH / SUPERUSER_SEARCH / SERVERFAULT_SEARCH
 *       api.stackexchange.com/2.3/search/advanced?site=<site>&q=
 *   COINCAP_ASSETS  api.coincap.io/v2/assets?search=<e> */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *OE3_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_se_site(const source_ctx *ctx, intel_sink *sink, const char *service,
                       const char *site, const char *enc) {
  char url[640];
  snprintf(url, sizeof url,
    "https://api.stackexchange.com/2.3/search/advanced?order=desc&sort=relevance&q=%s&site=%s", enc, site);
  const char *hdrs[] = { "Accept: application/json", OE3_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, service);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int n = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *q;
    cJSON_ArrayForEach(q, items) {
      const char *title = jo_sv(q, "title");
      const char *link = jo_sv(q, "link");
      const cJSON *score = cJSON_GetObjectItem(q, "score");
      if (!title) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (score && cJSON_IsNumber(score)) cJSON_AddNumberToObject(d, "score", score->valuedouble);
      cJSON_AddStringToObject(d, "site", site);
      cJSON_AddStringToObject(d, "source", service);
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", service); cJSON_AddStringToObject(p, "site", site);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
      intel_item it = {0};
      it.remote_key = link ? link : title; it.title = title; it.body = bj;
      it.link = link; it.lang = "en"; it.record_type = "stackexchange-question";
      it.properties_json = pj; it.tags_json = tags;
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
      if (n >= 20) break;
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "SECURITY_SE_SEARCH"))  n = run_se_site(ctx, sink, "SECURITY_SE_SEARCH", "security", enc);
  else if (!strcmp(sid, "SUPERUSER_SEARCH"))    n = run_se_site(ctx, sink, "SUPERUSER_SEARCH", "superuser", enc);
  else if (!strcmp(sid, "SERVERFAULT_SEARCH"))  n = run_se_site(ctx, sink, "SERVERFAULT_SEARCH", "serverfault", enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define OE3_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/extras3", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

OE3_DEF(oe3_secse_def, "SECURITY_SE_SEARCH", "Security StackExchange", "cyber",
  "Security Stack Exchange keyless question search (title, score).");
OE3_DEF(oe3_su_def, "SUPERUSER_SEARCH", "Super User Search", "cyber",
  "Super User keyless question search (title, score).");
OE3_DEF(oe3_sf_def, "SERVERFAULT_SEARCH", "Server Fault Search", "cyber",
  "Server Fault keyless question search (title, score).");
