/* collectors/sources/reg_ua_prozorro.c
 * OSINT service — UA_PROZORRO. On-demand entity pivot (entity = supplier /
 * buyer / product keyword) against Ukraine's Prozorro public procurement
 * search (prozorro.gov.ua/api/search/tenders). Keyless, LIVE. The search
 * endpoint is POST-only; GET returns 405. One intel_item per matched tender
 * keyed by tenderID. Emits real tender title, procuring entity (buyer),
 * value and status parsed from the response — no fabrication. The award/
 * announce date is not a separate field in the search shape; Prozorro encodes
 * it in tenderID as UA-YYYY-MM-DD-..., which we parse out. No matches / fetch
 * failure → emits nothing (honest empty). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define PROZORRO_URL "https://prozorro.gov.ua/api/search/tenders"

/* tenderID looks like "UA-2026-06-24-012448-a" — pull the YYYY-MM-DD prefix. */
static void prozorro_date(const char *tid, char *out, size_t outsz) {
  out[0] = 0;
  if (!tid) return;
  const char *p = tid;
  if (strncmp(p, "UA-", 3) == 0) p += 3;
  /* need digit-digit-digit-digit-'-'... */
  int ok = 1;
  for (int i = 0; i < 10; i++) {
    char c = p[i];
    if (i == 4 || i == 7) { if (c != '-') { ok = 0; break; } }
    else if (!isdigit((unsigned char)c)) { ok = 0; break; }
  }
  if (ok) snprintf(out, outsz, "%.10s", p);
}

/* one tender → one intel_item. Returns 1 if emitted. */
static int emit_tender(intel_sink *sink, const cJSON *t) {
  if (!t) return 0;
  const char *tid   = jo_sv(t, "tenderID");
  const char *title = jo_sv(t, "title");
  const char *status = jo_sv(t, "status");
  if (!tid && !title) return 0;

  const cJSON *pe = cJSON_GetObjectItem(t, "procuringEntity");
  const char *buyer = pe ? jo_sv(pe, "name") : NULL;
  const cJSON *ident = pe ? cJSON_GetObjectItem(pe, "identifier") : NULL;
  const char *buyer_legal = ident ? jo_sv(ident, "legalName") : NULL;
  const char *buyer_edrpou = ident ? jo_sv(ident, "id") : NULL;
  if (!buyer) buyer = buyer_legal;

  const cJSON *val = cJSON_GetObjectItem(t, "value");
  const cJSON *amt = val ? cJSON_GetObjectItem(val, "amount") : NULL;
  const char *cur = val ? jo_sv(val, "currency") : NULL;
  double amount = (amt && cJSON_IsNumber(amt)) ? amt->valuedouble : 0;

  char date[16]; prozorro_date(tid, date, sizeof date);

  char link[256] = {0};
  if (tid) snprintf(link, sizeof link, "https://prozorro.gov.ua/tender/%s", tid);

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (tid)   cJSON_AddStringToObject(data, "tender_id", tid);
  if (buyer) cJSON_AddStringToObject(data, "buyer", buyer);
  if (buyer_edrpou) cJSON_AddStringToObject(data, "buyer_edrpou", buyer_edrpou);
  if (amt && cJSON_IsNumber(amt)) cJSON_AddNumberToObject(data, "value_amount", amount);
  if (cur)    cJSON_AddStringToObject(data, "value_currency", cur);
  if (status) cJSON_AddStringToObject(data, "status", status);
  if (date[0]) cJSON_AddStringToObject(data, "date", date);
  cJSON_AddStringToObject(data, "source", "Prozorro");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "UA_PROZORRO");
  cJSON_AddStringToObject(props, "source", "Prozorro");
  if (tid)   cJSON_AddStringToObject(props, "tender_id", tid);
  if (buyer) cJSON_AddStringToObject(props, "buyer", buyer);
  if (buyer_edrpou) cJSON_AddStringToObject(props, "buyer_edrpou", buyer_edrpou);
  if (amt && cJSON_IsNumber(amt)) cJSON_AddNumberToObject(props, "value_amount", amount);
  if (cur)    cJSON_AddStringToObject(props, "value_currency", cur);
  if (status) cJSON_AddStringToObject(props, "status", status);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[512] = {0};
  if (buyer && amt && cJSON_IsNumber(amt))
    snprintf(summary, sizeof summary, "%s — %.0f %s", buyer, amount, cur ? cur : "");
  else if (amt && cJSON_IsNumber(amt))
    snprintf(summary, sizeof summary, "%.0f %s", amount, cur ? cur : "");
  else if (buyer)
    snprintf(summary, sizeof summary, "%s", buyer);

  intel_item it = {0};
  it.remote_key      = tid ? tid : title;
  it.title           = title ? title : tid;
  it.summary         = summary[0] ? summary : buyer;
  it.body            = bj;
  it.lang            = "uk";
  it.published_at    = date[0] ? date : NULL;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "prozorro-tender";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"UA_PROZORRO\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* Build JSON POST body: {"text":"<entity>"}. Escape " and \ in the query. */
  char text[512]; size_t tj = 0;
  for (const char *p = q; *p && tj < sizeof text - 2; p++) {
    if (*p == '"' || *p == '\\') text[tj++] = '\\';
    text[tj++] = *p;
  }
  text[tj] = 0;
  char body[640];
  int bn = snprintf(body, sizeof body, "{\"text\":\"%s\"}", text);

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };
  http_response hr = {0};
  int hc = http_request(ctx->http, "POST", PROZORRO_URL, hdrs,
                        body, (size_t)bn, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[ua_prozorro] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;

  cJSON *arr = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *t; cJSON_ArrayForEach(t, arr) emitted += emit_tender(sink, t);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[ua_prozorro] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def ua_prozorro_def = {
  .id = "UA_PROZORRO", .collector = "osint",
  .name = "Prozorro Procurement (Ukraine)", .name_ja = "プロゾロ 公共調達（ウクライナ）",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/ua_prozorro",
  .description = "Ukraine Prozorro open procurement search — tenders by supplier/buyer/keyword (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ua_prozorro_def)
