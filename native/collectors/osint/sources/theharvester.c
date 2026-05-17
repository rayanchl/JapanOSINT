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
 * 1000. Output {domain,emails[],subdomains[],summary{emails_found,
 * subdomains_found}} wrapped in the standard envelope; confidence 85 if any
 * hits else 60. Emits ONE osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#define MAX_RESULTS 1000
#define MAX_PAGES   10

typedef struct { char v[256]; char src[64]; } hit_t;

static void add_hit(hit_t *arr, int *n, const char *v, const char *src) {
  if (*n >= MAX_RESULTS) return;
  for (int i = 0; i < *n; i++) if (strcmp(arr[i].v, v) == 0) return;
  strncpy(arr[*n].v, v, 255); arr[*n].v[255] = 0;
  strncpy(arr[*n].src, src, 63); arr[*n].src[63] = 0;
  (*n)++;
}

static void extract(const char *text, const char *pattern, hit_t *arr, int *n,
                    const char *src) {
  regex_t re;
  if (regcomp(&re, pattern, REG_EXTENDED | REG_ICASE) != 0) return;
  regmatch_t m[1];
  const char *cur = text;
  while (regexec(&re, cur, 1, m, 0) == 0 && *n < MAX_RESULTS) {
    int len = (int)(m[0].rm_eo - m[0].rm_so);
    if (len >= 256) len = 255;
    char buf[256];
    memcpy(buf, cur + m[0].rm_so, len);
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

  char epat[512], spat[512];
  snprintf(epat, sizeof epat, "[a-zA-Z0-9._%%+-]+@%s", domain);
  snprintf(spat, sizeof spat,
           "([a-zA-Z0-9][a-zA-Z0-9-]{0,61}[a-zA-Z0-9]\\.)+%s", domain);

  /* search_google: 10 SERP pages */
  for (int page = 0; page < MAX_PAGES; page++) {
    char url[512];
    snprintf(url, sizeof url,
      "https://www.google.com/search?q=site:%s+OR+@%s&start=%d",
      domain, domain, page * 10);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, epat, emails, &ne, "Google");
             extract(b, spat, subs, &ns, "Google"); free(b); }
  }
  /* search_crtsh */
  {
    char url[512];
    snprintf(url, sizeof url, "https://crt.sh/?q=%%.%s&output=json", domain);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, spat, subs, &ns, "crt.sh"); free(b); }
  }
  /* search_github */
  {
    char url[512];
    snprintf(url, sizeof url, "https://api.github.com/search/code?q=%s", domain);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, epat, emails, &ne, "GitHub");
             extract(b, spat, subs, &ns, "GitHub"); free(b); }
  }
  /* search_duckduckgo */
  {
    char url[512];
    snprintf(url, sizeof url,
      "https://html.duckduckgo.com/html/?q=site:%s+OR+@%s", domain, domain);
    char *b = fetch(ctx->http, url);
    if (b) { extract(b, epat, emails, &ne, "DuckDuckGo");
             extract(b, spat, subs, &ns, "DuckDuckGo"); free(b); }
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "domain", domain);
  cJSON *ea = cJSON_CreateArray();
  for (int i = 0; i < ne; i++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "email", emails[i].v);
    cJSON_AddStringToObject(o, "source", emails[i].src);
    cJSON_AddItemToArray(ea, o);
  }
  cJSON_AddItemToObject(root, "emails", ea);
  cJSON *sa = cJSON_CreateArray();
  for (int i = 0; i < ns; i++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "subdomain", subs[i].v);
    cJSON_AddStringToObject(o, "source", subs[i].src);
    cJSON_AddItemToArray(sa, o);
  }
  cJSON_AddItemToObject(root, "subdomains", sa);
  cJSON *summ = cJSON_CreateObject();
  cJSON_AddNumberToObject(summ, "emails_found", ne);
  cJSON_AddNumberToObject(summ, "subdomains_found", ns);
  cJSON_AddItemToObject(root, "summary", summ);
  free(emails); free(subs);

  int conf = (ne + ns > 0) ? 85 : 60;
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", conf);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "EMAIL_HARVESTER");
  cJSON_AddStringToObject(props, "entity", domain);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", conf);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "harvester:%s", domain);
  char title[320]; snprintf(title, sizeof title, "EMAIL_HARVESTER — %s", domain);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (ne + ns > 0) ? "harvested emails/subdomains" : "no harvest results";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"EMAIL_HARVESTER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def theharvester_def = {
  .id = "EMAIL_HARVESTER", .collector = "osint",
  .name = "theHarvester", .name_ja = "theHarvester",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(theharvester_def)
