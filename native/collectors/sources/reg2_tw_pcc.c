/* collectors/sources/reg2_tw_pcc.c
 *
 * Taiwan government e-procurement (PCC) notices, via the g0v/openfun mirror of
 * web.pcc.gov.tw. NOTE: pcc.g0v.ronny.tw now 301-redirects to
 * pcc-api.openfun.app — the new host is used directly.
 *
 *  tw-pcc-tenders            GET https://pcc-api.openfun.app/api/listbydate?date=YYYYMMDD
 *    Scheduled daily sweep. Asks for today (UTC+8 procurement day); if that day
 *    has no records yet it retries the previous day once — both are real
 *    fetches, and an empty answer stays an empty answer.
 *
 *  TW_PCC_COMPANY_TENDERS    GET https://pcc-api.openfun.app/api/searchbycompanyname?query=<name>
 *    On-demand company pivot: every public tender a named company bid on or
 *    won. (The sibling /api/searchbycompanyid returns 0 for valid ids, so name
 *    search is used.) Paged via &page=; the first page is taken.
 *
 * Both share the same record shape, so one emitter serves both. Emitted per
 * record, all straight from the payload: brief.title, brief.type,
 * brief.category, unit_id, unit_name, date, filename, tender_api_url and
 * brief.companies.{ids,names} — the winning-company ids join to the Taiwanese
 * GCIS company register.
 *
 * Keyless. No coordinates are published, so no row claims geo.
 * Licence: g0v/openfun mirror of web.pcc.gov.tw notices, which are statutorily
 * public; the PCC is cited as the origin in every row.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

/* record's `date`, which arrives as 20260731 (number or string). */
static int tw_date(const cJSON *rec, char *iso, size_t n) {
  const cJSON *d = cJSON_GetObjectItem(rec, "date");
  char raw[16] = "";
  if (cJSON_IsNumber(d))      snprintf(raw, sizeof raw, "%.0f", d->valuedouble);
  else if (cJSON_IsString(d) && d->valuestring) snprintf(raw, sizeof raw, "%s", d->valuestring);
  if (strlen(raw) != 8) return 0;
  snprintf(iso, n, "%.4s-%.2s-%.2s", raw, raw + 4, raw + 6);
  return 1;
}

/* Emit every record in `arr`. Returns the number emitted. */
static int tw_emit(const cJSON *arr, const source_ctx *ctx, intel_sink *sink,
                   const char *service, const char *query, const char *link,
                   int cap) {
  (void)ctx;
  int n = 0;
  const cJSON *rec;
  cJSON_ArrayForEach(rec, arr) {
    if (n >= cap) break;
    if (!cJSON_IsObject(rec)) continue;
    const cJSON *brief = cJSON_GetObjectItem(rec, "brief");
    const char *title = brief ? jo_sv(brief, "title") : NULL;
    const char *type  = brief ? jo_sv(brief, "type") : NULL;
    const char *cat   = brief ? jo_sv(brief, "category") : NULL;
    const char *unit  = jo_sv(rec, "unit_name");
    if (!title && !unit) continue;             /* nothing real -> no row (R1) */

    const char *fname = jo_sv(rec, "filename");
    const char *uid   = jo_sv(rec, "unit_id");
    const char *api   = jo_sv(rec, "tender_api_url");
    char iso[16];
    int have_date = tw_date(rec, iso, sizeof iso);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", service);
    cJSON_AddStringToObject(props, "source", "web.pcc.gov.tw (via pcc-api.openfun.app)");
    if (query) cJSON_AddStringToObject(props, "query", query);
    if (title) cJSON_AddStringToObject(props, "title", title);
    if (type)  cJSON_AddStringToObject(props, "notice_type", type);
    if (cat)   cJSON_AddStringToObject(props, "category", cat);
    if (unit)  cJSON_AddStringToObject(props, "unit_name", unit);
    if (uid)   cJSON_AddStringToObject(props, "unit_id", uid);
    if (fname) cJSON_AddStringToObject(props, "filename", fname);
    if (api)   cJSON_AddStringToObject(props, "tender_api_url", api);
    if (have_date) cJSON_AddStringToObject(props, "date", iso);

    if (brief) {
      const cJSON *co = cJSON_GetObjectItem(brief, "companies");
      if (co) {
        const cJSON *ids = cJSON_GetObjectItem(co, "ids");
        const cJSON *nms = cJSON_GetObjectItem(co, "names");
        if (cJSON_IsArray(ids)) {
          cJSON *a = cJSON_CreateArray();
          const cJSON *v;
          cJSON_ArrayForEach(v, ids)
            if (cJSON_IsString(v) && v->valuestring)
              cJSON_AddItemToArray(a, cJSON_CreateString(v->valuestring));
          cJSON_AddItemToObject(props, "company_ids", a);
        }
        if (cJSON_IsArray(nms)) {
          cJSON *a = cJSON_CreateArray();
          const cJSON *v;
          cJSON_ArrayForEach(v, nms)
            if (cJSON_IsString(v) && v->valuestring)
              cJSON_AddItemToArray(a, cJSON_CreateString(v->valuestring));
          cJSON_AddItemToObject(props, "company_names", a);
        }
      }
    }
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[400];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             unit ? unit : "", (unit && type) ? " · " : "", type ? type : "",
             (cat && (unit || type)) ? " · " : "", cat ? cat : "");

    char key[256];
    if (fname) snprintf(key, sizeof key, "%s", fname);
    else       snprintf(key, sizeof key, "%s|%s|%s", uid ? uid : "",
                        have_date ? iso : "", title ? title : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title ? title : unit;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = api ? api : link;
    it.lang            = "zh";
    it.published_at    = have_date ? iso : NULL;
    it.record_type     = "tw-procurement-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"taiwan\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

/* ------------------------------------------------------- daily notice sweep */
static int tw_daily_run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  int n = 0;
  int fetched = 0;
  for (int back = 0; back <= 1 && n == 0; back++) {
    time_t t = now - (time_t)back * 24 * 3600;
    struct tm g;
    gmtime_r(&t, &g);
    char ymd[16];
    strftime(ymd, sizeof ymd, "%Y%m%d", &g);

    char url[160];
    snprintf(url, sizeof url,
             "https://pcc-api.openfun.app/api/listbydate?date=%s", ymd);
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) continue;
    fetched = 1;
    const cJSON *arr = cJSON_GetObjectItem(doc, "records");
    if (cJSON_IsArray(arr))
      n += tw_emit(arr, ctx, sink, "tw-pcc-tenders", NULL, url, 500);
    cJSON_Delete(doc);
  }
  if (!fetched) {
    fprintf(stderr, "[tw-pcc-tenders] fetch/parse failed\n");
    return -1;
  }
  fprintf(stderr, "[tw-pcc-tenders] emitted %d\n", n);
  return 0;
}

/* ------------------------------------------------------- company-name pivot */
static int tw_company_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  size_t qn = strlen(q);
  if (qn < 2 || qn > 160) return 0;
  if (strchr(q, '@') || strchr(q, '/')) return 0;    /* not a company name */

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[700];
  snprintf(url, sizeof url,
           "https://pcc-api.openfun.app/api/searchbycompanyname?query=%s", enc);
  free(enc);

  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return 0;                    /* upstream unavailable → empty */
  const cJSON *arr = cJSON_GetObjectItem(doc, "records");
  int n = 0;
  if (cJSON_IsArray(arr))
    n = tw_emit(arr, ctx, sink, "TW_PCC_COMPANY_TENDERS", q, url, 200);
  cJSON_Delete(doc);
  fprintf(stderr, "[TW_PCC_COMPANY_TENDERS] emitted %d\n", n);
  return 0;
}

static const source_def reg2_tw_pcc_daily_def = {
  .id = "tw-pcc-tenders", .collector = "government",
  .name = "Taiwan government e-procurement daily notices",
  .update_interval_sec = 43200, .run = tw_daily_run,
  .category = "government", .type = "api",
  .url = "https://pcc-api.openfun.app/api/listbydate",
  .description = "Every Taiwanese government tender, award and amendment notice "
                 "for a given day, with the procuring unit and — on award "
                 "notices — the winning company id and name. Keyless.",
  .license = "g0v/openfun mirror of web.pcc.gov.tw notices, which are "
             "statutorily public; the PCC is the origin.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_tw_pcc_daily_def)

static const source_def reg2_tw_pcc_company_def = {
  .id = "TW_PCC_COMPANY_TENDERS", .collector = "osint",
  .name = "Taiwan procurement — contracts won by a company",
  .update_interval_sec = 0, .run = tw_company_run,
  .category = "industry", .type = "api",
  .url = "https://pcc-api.openfun.app/api/searchbycompanyname",
  .description = "Given a Taiwanese company name, every public tender it bid on "
                 "or won — a direct company-to-state-contract pivot. Keyless.",
  .license = "g0v/openfun mirror of web.pcc.gov.tw notices, which are "
             "statutorily public; the PCC is the origin.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_tw_pcc_company_def)
