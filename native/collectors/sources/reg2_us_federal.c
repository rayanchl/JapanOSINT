/* collectors/sources/reg2_us_federal.c
 *
 * Two US federal award sources.
 *
 *  us-federal-award-search — USAspending award search (scheduled sweep)
 *    POST https://api.usaspending.gov/api/v2/search/spending_by_award/
 *    body {"filters":{"award_type_codes":["A","B","C","D"],
 *                     "time_period":[{"start_date":..,"end_date":..}]},
 *          "fields":[...],"page":1,"limit":100,"sort":"Award Amount","order":"desc"}
 *    (the API rejects a start_date before 2007-10-01; we ask for a rolling
 *     30-day window ending today, clamped to that floor.)
 *    Emits, all straight out of results[]: "Award ID", "Recipient Name",
 *    "Award Amount", "Awarding Agency", "Start Date", "generated_internal_id"
 *    (the key for the GET /api/v2/awards/{id}/ detail lookup).
 *
 *  us-fpds-contracts — FPDS-NG public Atom feed (on-demand company pivot)
 *    GET https://www.fpds.gov/ezsearch/FEEDS/ATOM?FEEDNAME=PUBLIC
 *        &q=VENDOR_FULL_NAME%3A%22<vendor>%22&templateName=1.5.3
 *    Emits per Atom <entry>: title (which already carries contract number,
 *    vendor and obligated amount), alternate link, <modified>/<updated> date,
 *    and — when the ns1 award XML in <content> uses the documented prefix —
 *    PIID, vendorName, obligatedAmount and signedDate. Fields that are not
 *    present are simply omitted; nothing is filled in.
 *
 * Keyless. No coordinates are published by either endpoint, so no row ever
 * claims geo. Empty result set => return 0 (a company with no federal awards
 * is a clean answer, not a failure).
 *
 * Licence: US Government work, public domain (Treasury DATA Act / FPDS-NG).
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

/* ------------------------------------------------ USAspending award search */

static int usa_run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  time_t from = now - 30 * 24 * 3600;
  struct tm gt, gf;
  gmtime_r(&now, &gt);
  gmtime_r(&from, &gf);
  char end_date[16], start_date[16];
  strftime(end_date, sizeof end_date, "%Y-%m-%d", &gt);
  strftime(start_date, sizeof start_date, "%Y-%m-%d", &gf);
  /* API floor: time_period must start on/after 2007-10-01. */
  if (strcmp(start_date, "2007-10-01") < 0)
    snprintf(start_date, sizeof start_date, "%s", "2007-10-01");

  char body[900];
  snprintf(body, sizeof body,
    "{\"filters\":{\"award_type_codes\":[\"A\",\"B\",\"C\",\"D\"],"
    "\"time_period\":[{\"start_date\":\"%s\",\"end_date\":\"%s\"}]},"
    "\"fields\":[\"Award ID\",\"Recipient Name\",\"Award Amount\","
    "\"Awarding Agency\",\"Awarding Sub Agency\",\"Start Date\",\"End Date\"],"
    "\"page\":1,\"limit\":100,\"sort\":\"Award Amount\",\"order\":\"desc\"}",
    start_date, end_date);

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };
  cJSON *doc = feed_post_json(ctx->http,
                              "https://api.usaspending.gov/api/v2/search/spending_by_award/",
                              body, hdrs, 30000);
  if (!doc) {
    fprintf(stderr, "[us-federal-award-search] fetch/parse failed\n");
    return -1;
  }

  const cJSON *arr = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    const char *recipient = jo_sv(r, "Recipient Name");
    const char *awid      = jo_sv(r, "Award ID");
    if (!recipient && !awid) continue;             /* nothing real -> no row */

    const char *agency  = jo_sv(r, "Awarding Agency");
    const char *sub     = jo_sv(r, "Awarding Sub Agency");
    const char *start   = jo_sv(r, "Start Date");
    const char *gid     = jo_sv(r, "generated_internal_id");
    const cJSON *amt    = cJSON_GetObjectItem(r, "Award Amount");
    int has_amt         = cJSON_IsNumber(amt);

    char title[400];
    snprintf(title, sizeof title, "%s%s%s",
             recipient ? recipient : awid,
             (recipient && awid) ? " — award " : "",
             (recipient && awid) ? awid : "");

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "us-federal-award-search");
    cJSON_AddStringToObject(props, "source", "api.usaspending.gov");
    if (awid)      cJSON_AddStringToObject(props, "award_id", awid);
    if (recipient) cJSON_AddStringToObject(props, "recipient_name", recipient);
    if (agency)    cJSON_AddStringToObject(props, "awarding_agency", agency);
    if (sub)       cJSON_AddStringToObject(props, "awarding_sub_agency", sub);
    if (start)     cJSON_AddStringToObject(props, "start_date", start);
    if (jo_sv(r, "End Date"))
      cJSON_AddStringToObject(props, "end_date", jo_sv(r, "End Date"));
    if (has_amt)   cJSON_AddNumberToObject(props, "award_amount", amt->valuedouble);
    if (gid)       cJSON_AddStringToObject(props, "generated_internal_id", gid);
    cJSON_AddStringToObject(props, "window_start", start_date);
    cJSON_AddStringToObject(props, "window_end", end_date);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char summary[400];
    if (has_amt)
      snprintf(summary, sizeof summary, "%s%s$%.2f",
               agency ? agency : "", agency ? " · " : "", amt->valuedouble);
    else
      snprintf(summary, sizeof summary, "%s", agency ? agency : "");

    char link[300];
    if (gid) snprintf(link, sizeof link, "https://www.usaspending.gov/award/%s", gid);
    else     snprintf(link, sizeof link, "https://www.usaspending.gov/search");

    intel_item it = {0};
    it.remote_key      = gid ? gid : (awid ? awid : title);
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = pj;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = start;
    it.record_type     = "federal-award";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"usa\",\"award\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[us-federal-award-search] emitted %d (%s..%s)\n",
          n, start_date, end_date);
  return 0;
}

/* -------------------------------------------------------- FPDS-NG Atom feed */

/* Minimal XML entity unescape, in place. */
static void fpds_unescape(char *s) {
  char *w = s;
  for (char *p = s; *p; ) {
    if (*p == '&') {
      if (strncmp(p, "&amp;", 5) == 0)       { *w++ = '&';  p += 5; continue; }
      if (strncmp(p, "&lt;", 4) == 0)        { *w++ = '<';  p += 4; continue; }
      if (strncmp(p, "&gt;", 4) == 0)        { *w++ = '>';  p += 4; continue; }
      if (strncmp(p, "&quot;", 6) == 0)      { *w++ = '"';  p += 6; continue; }
      if (strncmp(p, "&apos;", 6) == 0)      { *w++ = '\''; p += 6; continue; }
      if (strncmp(p, "&#39;", 5) == 0)       { *w++ = '\''; p += 5; continue; }
    }
    *w++ = *p++;
  }
  *w = 0;
}

/* Inner text of the first <tag>…</tag> inside [buf,end); malloc'd or NULL. */
static char *fpds_tag(const char *buf, const char *tag) {
  const char *cur = buf;
  char *v = jo_tag_inner(&cur, tag);
  if (v) fpds_unescape(v);
  if (v && !v[0]) { free(v); return NULL; }
  return v;
}

static int fpds_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  size_t qn = strlen(q);
  if (qn < 3 || qn > 120) return 0;                 /* wrong shape -> no-op */
  if (strchr(q, '@') || strchr(q, '/')) return 0;   /* not a vendor name */

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[700];
  snprintf(url, sizeof url,
           "https://www.fpds.gov/ezsearch/FEEDS/ATOM?FEEDNAME=PUBLIC"
           "&q=VENDOR_FULL_NAME%%3A%%22%s%%22&templateName=1.5.3", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/atom+xml, application/xml", NULL };
  char *xml = jo_get(ctx, url, hdrs, "us-fpds-contracts");
  if (!xml) return 0;                     /* upstream unavailable -> empty */

  int n = 0;
  const char *cur = xml;
  while (n < 50) {
    const char *hit = jo_next_entry(&cur);
    if (!hit) break;
    const char *close = strstr(hit, "</entry>");
    if (!close) break;
    size_t len = (size_t)(close - hit);
    char *chunk = (char *)malloc(len + 1);
    if (!chunk) break;
    memcpy(chunk, hit, len);
    chunk[len] = 0;
    cur = close + 8;

    char *title = fpds_tag(chunk, "title");
    if (!title) { free(chunk); continue; }   /* no real title -> no row (R1) */

    char *modified = fpds_tag(chunk, "modified");
    if (!modified) modified = fpds_tag(chunk, "updated");
    char *piid     = fpds_tag(chunk, "ns1:PIID");
    char *vendor   = fpds_tag(chunk, "ns1:vendorName");
    char *amount   = fpds_tag(chunk, "ns1:obligatedAmount");
    char *signed_d = fpds_tag(chunk, "ns1:signedDate");

    /* alternate link href */
    char link[900] = "";
    const char *h = strstr(chunk, "href=\"");
    if (h) {
      h += 6;
      const char *he = strchr(h, '"');
      if (he && he - h < (long)sizeof link) {
        snprintf(link, sizeof link, "%.*s", (int)(he - h), h);
        fpds_unescape(link);
      }
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "us-fpds-contracts");
    cJSON_AddStringToObject(props, "source", "fpds.gov");
    cJSON_AddStringToObject(props, "query", q);
    cJSON_AddStringToObject(props, "notice", title);
    if (piid)     cJSON_AddStringToObject(props, "piid", piid);
    if (vendor)   cJSON_AddStringToObject(props, "vendor_name", vendor);
    if (amount)   cJSON_AddStringToObject(props, "obligated_amount", amount);
    if (signed_d) cJSON_AddStringToObject(props, "signed_date", signed_d);
    if (modified) cJSON_AddStringToObject(props, "modified", modified);
    if (link[0])  cJSON_AddStringToObject(props, "href", link);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = piid ? piid : (link[0] ? link : title);
    it.title           = title;
    it.summary         = vendor ? vendor : NULL;
    it.body            = pj;
    it.link            = link[0] ? link : url;
    it.lang            = "en";
    it.published_at    = signed_d ? signed_d : modified;
    it.record_type     = "federal-contract-award";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"procurement\",\"usa\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(title); free(modified); free(piid);
    free(vendor); free(amount); free(signed_d);
    free(chunk);
  }

  free(xml);
  fprintf(stderr, "[us-fpds-contracts] emitted %d\n", n);
  return 0;
}

static const source_def reg2_usaspending_def = {
  .id = "us-federal-award-search", .collector = "government",
  .name = "USAspending federal award search",
  .update_interval_sec = 21600, .run = usa_run,
  .category = "government", .type = "api",
  .url = "https://api.usaspending.gov/api/v2/search/spending_by_award/",
  .description = "US federal contract, grant and loan awards from the Treasury "
                 "DATA Act store — recipient, awarding agency and dollar amount "
                 "over a rolling 30-day window. Keyless.",
  .license = "US Government work, public domain (Treasury DATA Act data).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_usaspending_def)

static const source_def reg2_fpds_def = {
  .id = "us-fpds-contracts", .collector = "osint",
  .name = "FPDS-NG federal contract award feed",
  .update_interval_sec = 0, .run = fpds_run,
  .category = "government", .type = "api",
  .url = "https://www.fpds.gov/ezsearch/FEEDS/ATOM",
  .description = "Raw Federal Procurement Data System award notices pivoted on "
                 "a vendor name — the authoritative US contract-award record "
                 "behind USAspending. Keyless.",
  .license = "US Government work, public domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_fpds_def)
