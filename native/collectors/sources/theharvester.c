/* collectors/osint/sources/theharvester.c
 * OSINT service — port of OSINTsaas osint_tools/theharvester.c
 * (theharvester_search_domain → handle_email_harvester). Canonical SERVICE =
 * EMAIL_HARVESTER — osint_dispatcher.c:141 {SERVICE_EMAIL_HARVESTER,
 * handle_email_harvester, "EMAIL_HARVESTER", true}; that handler calls
 * theharvester_search_domain (this file is the port-FROM for EMAIL_HARVESTER).
 * On-demand (interval 0); ctx->entity = a domain. No key. Faithfully
 * reproduces theharvester_search(): search_google (10 SERP pages),
 * search_crtsh (crt.sh JSON text), search_github (api.github.com/search/code),
 * search_duckduckgo (html.duckduckgo.com) — each scraped with the SAME email
 * regex "[a-zA-Z0-9._%+-]+@<domain>" and subdomain regex
 * "([a-zA-Z0-9][a-zA-Z0-9-]{0,61}[a-zA-Z0-9]\\.)+<domain>", de-duplicated, cap
 * 1000.
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per UNIQUE harvested
 * email AND per unique subdomain (not a single summary blob). Each hit is
 * de-duplicated within the run (add_hit) and the sources it was found via are
 * merged. remote_key = "email:<addr>" | "subdomain:<host>"; title = the
 * email/subdomain; body = {value,type,found_via:[...]}. If nothing is
 * harvested, emits nothing and returns 0 (honest empty). */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

/* NOT a request budget, and nothing logged it: this is the size of the emails[]
 * and subs[] tables. When one fills, add_hit() drops further values and
 * extract() stops scanning the page at all, so hits past the 1000th are lost
 * without a trace. run() now emits a collector-truncation-notice when it bites.
 * records_available cannot be known there and must stay -1: once the table is
 * full there is nothing left to de-duplicate a further match against, so any
 * "remaining" count would be a guess. */
#define MAX_RESULTS 1000  /* exhaustive-ok: result-table size; overflow is
                           * reported as a truncation notice, not dropped */
#define MAX_PAGES   10    /* exhaustive-ok: page-walk runaway guard */

typedef struct { char v[256]; char src[256]; } hit_t;

/* De-dup within the run. On a repeat value, merge the discovering source into
 * a comma-separated `src` list (skip if already present). */
static void add_hit(hit_t *arr, int *n, const char *v, const char *src) {
  for (int i = 0; i < *n; i++) {
    if (strcmp(arr[i].v, v) == 0) {
      if (!strstr(arr[i].src, src)) {
        size_t cur = strlen(arr[i].src);
        if (cur && cur + strlen(src) + 2 < sizeof arr[i].src)
          snprintf(arr[i].src + cur, sizeof arr[i].src - cur, ",%s", src);
      }
      return;
    }
  }
  if (*n >= MAX_RESULTS) return;
  snprintf(arr[*n].v, sizeof arr[*n].v, "%s", v);
  snprintf(arr[*n].src, sizeof arr[*n].src, "%s", src);
  (*n)++;
}

/* Emit ONE intel row for a single harvested value. type = "email"|"subdomain".
 * srclist = comma-separated discovering sources. Returns 1 if emitted. */
static int emit_hit(intel_sink *sink, const char *domain, const char *value,
                    const char *type, const char *srclist) {
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "value", value);
  cJSON_AddStringToObject(body, "type", type);
  cJSON *via = cJSON_CreateArray();
  char tmp[256]; strncpy(tmp, srclist, 255); tmp[255] = 0;
  char *save = NULL;              /* strtok_r: concurrent workers, see jsonlist.c */
  for (char *tok = strtok_r(tmp, ",", &save); tok; tok = strtok_r(NULL, ",", &save))
    cJSON_AddItemToArray(via, cJSON_CreateString(tok));
  cJSON_AddItemToObject(body, "found_via", via);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EMAIL_HARVESTER");
  cJSON_AddStringToObject(props, "entity", domain);
  cJSON_AddStringToObject(props, "type", type);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320], summary[320];
  snprintf(rk, sizeof rk, "%s:%s", type, value);
  snprintf(summary, sizeof summary, "%s %s for %s", type, value, domain);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = value;
  it.body            = bj;
  it.summary         = summary;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"EMAIL_HARVESTER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* POSIX ERE has no look-behind, so a match that starts in the middle of a
 * longer token is accepted by regexec. On percent-encoded SERP links
 * (…%2Fwww.example.com) that yielded junk "subdomains" like "2Fwww.example.com".
 * Reject a match whose preceding byte could still be part of the host token. */
static int boundary_ok(const char *text, const char *start) {
  if (start <= text) return 1;
  unsigned char c = (unsigned char)start[-1];
  return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' ||
           c == '%' || c == '@');
}

static void extract(const char *text, const char *pattern, hit_t *arr, int *n,
                    const char *src) {
  regex_t re;
  if (regcomp(&re, pattern, REG_EXTENDED | REG_ICASE) != 0) return;
  regmatch_t m[1];
  const char *cur = text;
  while (regexec(&re, cur, 1, m, 0) == 0 && *n < MAX_RESULTS) {
    const char *start = cur + m[0].rm_so;
    if (!boundary_ok(text, start)) { cur = start + 1; continue; }
    int len = (int)(m[0].rm_eo - m[0].rm_so);
    if (len >= 256) len = 255;
    char buf[256];
    memcpy(buf, start, len);
    buf[len] = 0;
    add_hit(arr, n, buf, src);
    cur += m[0].rm_eo;
  }
  regfree(&re);
}

static char *fetch(http_client *http, const char *url) {
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  char *body = (hc == 0 && hr.status == 200 && hr.body) ? strdup(hr.body) : NULL;
  http_response_free(&hr);
  return body;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;

  hit_t *emails = calloc(MAX_RESULTS, sizeof(hit_t));
  hit_t *subs = calloc(MAX_RESULTS, sizeof(hit_t));
  if (!emails || !subs) { free(emails); free(subs); return -1; }
  int ne = 0, ns = 0;

  /* The domain goes into an ERE, so its dots must be escaped — unescaped they
   * are "any char" and matched hosts like "toyotaXcoYjp". */
  char dre[256]; size_t dw = 0;
  for (const char *d = domain; *d && dw + 2 < sizeof dre; d++) {
    if (!((*d >= 'a' && *d <= 'z') || (*d >= 'A' && *d <= 'Z') ||
          (*d >= '0' && *d <= '9') || *d == '-')) dre[dw++] = '\\';
    dre[dw++] = *d;
  }
  dre[dw] = 0;

  /* The same entity also goes into four query strings, so it needs the other
   * escape too: percent-encoding. Raw, an entity containing '&' or '=' stopped
   * being a search TERM and became extra request parameters (…?q=site:x&num=1
   * →  a second parameter we never intended to send), and '#' truncated the
   * URL. Encode once here and interpolate `enc` — never `domain` — into a URL.
   * The regex above keeps using the raw domain: that is a different escape. */
  char enc[512];
  jo_urlencode_buf(domain, enc, sizeof enc);

  char epat[512], spat[512];
  snprintf(epat, sizeof epat, "[a-zA-Z0-9._%%+-]+@%s", dre);
  snprintf(spat, sizeof spat,
           "([a-zA-Z0-9][a-zA-Z0-9-]{0,61}[a-zA-Z0-9]\\.)+%s", dre);

  /* search_google: 10 SERP pages */
  for (int page = 0; page < MAX_PAGES; page++) {
    char url[2048];
    snprintf(url, sizeof url,
      "https://www.google.com/search?q=site:%s+OR+@%s&start=%d",
      enc, enc, page * 10);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, epat, emails, &ne, "Google");
             extract(b, spat, subs, &ns, "Google"); free(b); }
  }
  /* search_crtsh */
  {
    char url[2048];
    snprintf(url, sizeof url, "https://crt.sh/?q=%%.%s&output=json", enc);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, spat, subs, &ns, "crt.sh"); free(b); }
  }
  /* search_github */
  {
    char url[2048];
    snprintf(url, sizeof url, "https://api.github.com/search/code?q=%s", enc);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, epat, emails, &ne, "GitHub");
             extract(b, spat, subs, &ns, "GitHub"); free(b); }
  }
  /* search_duckduckgo */
  {
    char url[2048];
    snprintf(url, sizeof url,
      "https://html.duckduckgo.com/html/?q=site:%s+OR+@%s", enc, enc);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, epat, emails, &ne, "DuckDuckGo");
             extract(b, spat, subs, &ns, "DuckDuckGo"); free(b); }
  }

  /* PER-RECORD: one row per unique harvested email, one per unique subdomain. */
  int emitted = 0;
  for (int i = 0; i < ne; i++)
    emitted += emit_hit(sink, domain, emails[i].v, "email", emails[i].src);
  for (int i = 0; i < ns; i++)
    emitted += emit_hit(sink, domain, subs[i].v, "subdomain", subs[i].src);

  free(emails); free(subs);
  (void)emitted;            /* nothing harvested → emitted nothing, honest 0 */

  /* House rule 2: a full table means the scan stopped early and threw matches
   * away. Say so in the data. -1 is the only honest `available` here — see the
   * note on MAX_RESULTS. */
  char tq[320];
  if (ne >= MAX_RESULTS) {
    /* Distinct query strings: the notice uid is derived per (source, query),
     * so a shared one would make the second notice overwrite the first. */
    snprintf(tq, sizeof tq, "%.280s (emails)", domain);
    jo_truncation_notice(sink, "EMAIL_HARVESTER", tq, ne, -1,
                         "the emails[] result table (MAX_RESULTS = 1000) "
                         "filled; extract() then stopped scanning the fetched "
                         "pages, so any further addresses on them were neither "
                         "de-duplicated nor emitted",
                         "raise MAX_RESULTS in collectors/sources/"
                         "theharvester.c, or grow the hit table instead of "
                         "fixing its size");
  }
  if (ns >= MAX_RESULTS) {
    snprintf(tq, sizeof tq, "%.280s (subdomains)", domain);
    jo_truncation_notice(sink, "EMAIL_HARVESTER", tq, ns, -1,
                         "the subs[] result table (MAX_RESULTS = 1000) filled; "
                         "extract() then stopped scanning the fetched pages, "
                         "so any further subdomains on them were neither "
                         "de-duplicated nor emitted",
                         "raise MAX_RESULTS in collectors/sources/"
                         "theharvester.c, or grow the hit table instead of "
                         "fixing its size");
  }
  return 0;
}

static const source_def theharvester_def = {
  .id = "EMAIL_HARVESTER", .collector = "osint",
  .name = "theHarvester", .name_ja = "theHarvester",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/email-harvester",
  .description = "Harvest emails/subdomains/hosts for a domain.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(theharvester_def)
