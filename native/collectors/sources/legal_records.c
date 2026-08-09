/* collectors/osint/sources/legal_records.c — legal/court-record OSINT, ported
 * from OSINTsaas court_records.c onto the JapanOSINT source ABI.
 *
 *   LEGAL_SEARCH      — general case-law / court opinions mentioning the entity
 *   BANKRUPTCY_SEARCH — court records narrowed with a bankruptcy term
 *   CRIMINAL_RECORDS  — court records / opinions mentioning the name
 *
 * Backed by CourtListener (free; optional COURTLISTENER_API_KEY raises limits).
 * The OSINTsaas tool also lists many anti-bot government portals + paid
 * UniCourt — not ported (would be bare links / paywalled). Honest-empty on no
 * match. CourtListener is US case law; results are court cases referencing the
 * query, surfaced verbatim — never fabricated. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int courtlistener(const source_ctx *ctx, intel_sink *sink,
                         const char *entity, const char *q, const char *service,
                         const char *tags) {
  char enc[512]; jo_urlencode_buf(q, enc, sizeof enc);
  char url[640];
  snprintf(url, sizeof url,
    "https://www.courtlistener.com/api/rest/v4/search/?q=%s&type=o", enc);
  /* CourtListener works unauthenticated (rate-limited); a token raises limits. */
  const char *key = getenv("COURTLISTENER_API_KEY");
  char auth[160] = {0};
  const char *headers[2] = { NULL, NULL };
  if (key && *key) { snprintf(auth, sizeof auth, "Authorization: Token %s", key); headers[0] = auth; }

  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers[0] ? headers : NULL, NULL, 0, 15000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  cJSON *res = cJSON_GetObjectItem(j, "results");
  int emitted = 0, n = (res && cJSON_IsArray(res)) ? cJSON_GetArraySize(res) : 0;
  for (int i = 0; i < n && i < 15; i++) {
    cJSON *r = cJSON_GetArrayItem(res, i);
    const char *cn = jo_str(r, "caseName");
    const char *court = jo_str(r, "court");
    const char *date = jo_str(r, "dateFiled");
    const char *rel = jo_str(r, "absolute_url");
    if (!cn) continue;
    char link[512] = {0};
    if (rel) snprintf(link, sizeof link, "https://www.courtlistener.com%s", rel);

    cJSON *out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "source", "courtlistener.com");
    cJSON_AddStringToObject(out, "case_name", cn);
    if (court) cJSON_AddStringToObject(out, "court", court);
    if (date) cJSON_AddStringToObject(out, "date_filed", date);
    if (*link) cJSON_AddStringToObject(out, "url", link);
    char *bj = cJSON_PrintUnformatted(out);
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", service);
    cJSON_AddStringToObject(props, "entity", entity);
    char *pj = cJSON_PrintUnformatted(props);
    char rk[400], title[400];
    snprintf(rk, sizeof rk, "legal:%s:%s", service, *link ? link : cn);
    snprintf(title, sizeof title, "%s — %s", cn, court ? court : "court record");
    intel_item it = {0};
    it.remote_key = rk; it.title = title; it.body = bj; it.summary = cn;
    it.link = *link ? link : NULL; it.record_type = "osint_service_result";
    it.properties_json = pj; it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
  }
  cJSON_Delete(j);
  return emitted;
}

/* run() is a STATUS code, not a row count: core/scheduler.c does
 * `status = rc == 0 ? "ok" : "error"`, so returning the emitted count marked
 * every successful search as an errored run and fed the anomaly detector. */
static int run_legal(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return 0;
  courtlistener(ctx, sink, ctx->entity, ctx->entity, "LEGAL_SEARCH",
                "[\"osint-search\",\"LEGAL_SEARCH\"]");
  return 0;
}
static int run_bankruptcy(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return 0;
  char q[300]; snprintf(q, sizeof q, "%s bankruptcy", ctx->entity);
  courtlistener(ctx, sink, ctx->entity, q, "BANKRUPTCY_SEARCH",
                "[\"osint-search\",\"BANKRUPTCY_SEARCH\"]");
  return 0;
}
static int run_criminal(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return 0;
  courtlistener(ctx, sink, ctx->entity, ctx->entity, "CRIMINAL_RECORDS",
                "[\"osint-search\",\"CRIMINAL_RECORDS\"]");
  return 0;
}

static const source_def legal_def = {
  .id = "LEGAL_SEARCH", .collector = "osint", .name = "Legal Search", .run = run_legal,
  .category = "investigation", .type = "api", .url = "https://www.courtlistener.com/",
  .description = "Court opinions / case law mentioning the entity (CourtListener, free).",
  .free_tier = 1,
};
REGISTER_SOURCE(legal_def)

static const source_def bankruptcy_def = {
  .id = "BANKRUPTCY_SEARCH", .collector = "osint", .name = "Bankruptcy Search", .run = run_bankruptcy,
  .category = "investigation", .type = "api", .url = "https://www.courtlistener.com/",
  .description = "Court records narrowed to bankruptcy proceedings (CourtListener, free).",
  .free_tier = 1,
};
REGISTER_SOURCE(bankruptcy_def)

static const source_def criminal_def = {
  .id = "CRIMINAL_RECORDS", .collector = "osint", .name = "Criminal/Court Records", .run = run_criminal,
  .category = "investigation", .type = "api", .url = "https://www.courtlistener.com/",
  .description = "US court cases / opinions referencing a name (CourtListener, free).",
  .free_tier = 1,
};
REGISTER_SOURCE(criminal_def)
