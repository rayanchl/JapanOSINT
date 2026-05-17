/* collectors/osint/sources/subdomain_osint.c
 * OSINT service — faithful port of OSINTsaas osint_tools/subdomain_osint.c
 * (handle_subdomain_finder → sublist3r_enumerate_domain →
 * subdomain_finder_service / subdomain_osint_enumerate). Canonical SERVICE
 * name in osint_dispatcher.c service_registry[] bound to
 * handle_subdomain_finder is "SUBDOMAIN_FINDER" (line 178). (sublist3r.c is
 * the same code path — sublist3r_enumerate_domain just calls
 * subdomain_finder_service — so there is no separate sublist3r service.)
 * On-demand (interval 0); dispatcher runs it with ctx->entity = a domain.
 *
 * Reproduces subdomain_osint_enumerate() faithfully: Phase 1 is Certificate
 * Transparency via crt.sh (?q=%.<domain>&output=json), splitting each
 * name_value on newlines, stripping leading spaces/'*', keeping names that
 * contain the domain and are longer than it, deduplicated. Phase 2 fetches
 * each source in search_sources[] (CRT.SH/Certspotter/HackerTarget/
 * Threatminer/ThreatCrowd/AlienVault/UrlScan/DNSDumpster/WaybackMachine/
 * Google/Bing/Yahoo/DuckDuckGo) and runs the source's POSIX-extended regex
 * over the body (REG_EXTENDED|REG_ICASE), accepting matches that contain the
 * domain, with global cross-source dedup — identical to the upstream worker
 * (run sequentially here instead of the 10-thread split; the set of results
 * is the same). The upstream LLaMA page-analysis fallback
 * (llama_analyze_page_for_query) is OSINTsaas-only page-analysis infra; it
 * only triggers when a source yields zero regex matches, so omitting it is
 * the graceful no-fallback path (no fabricated data). Output uses the
 * result_builder envelope, reproduced exactly: { query, query_type:"domain",
 * results:[{source, found:true, confidence, detection_method:"direct",
 * data:{subdomain,source,verified:false}, llama_analysis:null}],
 * summary:{total_sources,found_count,detection_breakdown}, _meta }.
 * success=true, confidence 90 if any found else 60. No API key. Emits one
 * osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SUBDOMAINS    5000
#define CONFIDENCE_HIGH   90
#define DETECTION_DIRECT  "direct"

typedef struct { char subdomain[256]; char source[64]; } sub_t;

typedef struct { const char *name, *search_url, *pattern; } search_source_t;

/* identical source list/patterns to upstream search_sources[] ({} = domain) */
static const search_source_t search_sources[] = {
  {"CRT.SH", "https://crt.sh/?q=%.{}&output=json", "\"name_value\":\"([^\"]+)\""},
  {"Certspotter", "https://api.certspotter.com/v1/issuances?domain={}&include_subdomains=true&expand=dns_names", "([a-zA-Z0-9.-]+\\.{})"},
  {"HackerTarget", "https://api.hackertarget.com/hostsearch/?q={}", "([a-zA-Z0-9.-]+)\\s*,\\s*[0-9.]+"},
  {"Threatminer", "https://api.threatminer.org/v2/domain.php?q={}&rt=5", "\"subdomains\":\\[([^\\]]+)\\]"},
  {"ThreatCrowd", "https://www.threatcrowd.org/searchApi/v2/domain/report/?domain={}", "\"subdomains\":\\[([^\\]]+)\\]"},
  {"AlienVault", "https://otx.alienvault.com/api/v1/indicators/domain/{}/passive_dns", "\"hostname\":\"([^\"]+)\""},
  {"UrlScan", "https://urlscan.io/api/v1/search/?q=domain:{}", "\"domain\":\"([a-zA-Z0-9.-]+\\.{})\""},
  {"DNSDumpster", "https://dnsdumpster.com/static/api/?q={}", "([a-zA-Z0-9.-]+\\.{})"},
  {"WaybackMachine", "https://web.archive.org/cdx/search/cdx?url=*.{}&output=json&collapse=urlkey", "([a-zA-Z0-9.-]+\\.{})"},
  {"Google", "https://www.google.com/search?q=site:{}&num=100", "([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+{}"},
  {"Bing", "https://www.bing.com/search?q=site:{}&count=100", "([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+{}"},
  {"Yahoo", "https://search.yahoo.com/search?p=site:{}", "([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+{}"},
  {"DuckDuckGo", "https://duckduckgo.com/html/?q=site:{}", "([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+{}"},
};

/* replace every "{}" in template with domain (upstream replace_domain_placeholder) */
static void build_url(char *dest, size_t cap, const char *tmpl, const char *domain) {
  size_t w = 0;
  size_t dl = strlen(domain);
  for (const char *p = tmpl; *p && w + 1 < cap; ) {
    if (p[0] == '{' && p[1] == '}') {
      if (w + dl < cap) { memcpy(dest + w, domain, dl); w += dl; }
      p += 2;
    } else {
      dest[w++] = *p++;
    }
  }
  dest[w] = '\0';
}

static int already(const sub_t *r, int n, const char *s) {
  for (int i = 0; i < n; i++) if (strcmp(r[i].subdomain, s) == 0) return 1;
  return 0;
}

/* Phase 1: crt.sh JSON name_value split on '\n' (upstream search_crtsh) */
static int search_crtsh(http_client *http, const char *domain, sub_t *r, int max) {
  int found = 0;
  char url[512];
  snprintf(url, sizeof url, "https://crt.sh/?q=%%.%s&output=json", domain);
  cJSON *json = feed_get_json(http, url, 30000);
  if (json && cJSON_IsArray(json)) {
    int len = cJSON_GetArraySize(json);
    for (int i = 0; i < len && found < max; i++) {
      cJSON *cert = cJSON_GetArrayItem(json, i);
      cJSON *nv = cJSON_GetObjectItem(cert, "name_value");
      if (cJSON_IsString(nv) && nv->valuestring) {
        char *copy = strdup(nv->valuestring);
        char *save = NULL;
        char *name = strtok_r(copy, "\n", &save);
        while (name && found < max) {
          while (*name == ' ' || *name == '*') name++;
          if (strstr(name, domain) && strlen(name) > strlen(domain) &&
              strlen(name) < 255 && !already(r, found, name)) {
            strcpy(r[found].subdomain, name);
            strcpy(r[found].source, "CRT.SH");
            found++;
          }
          name = strtok_r(NULL, "\n", &save);
        }
        free(copy);
      }
    }
  }
  if (json) cJSON_Delete(json);
  return found;
}

/* one source: fetch + POSIX-extended regex extract (upstream worker step 1) */
static void search_source(http_client *http, const search_source_t *src,
                          const char *domain, sub_t *results, int *count) {
  char url[2048];
  build_url(url, sizeof url, src->search_url, domain);

  char *body = feed_get_text(http, url, 30000);
  if (!body) return;

  regex_t re;
  if (regcomp(&re, src->pattern, REG_EXTENDED | REG_ICASE) != 0) { free(body); return; }

  regmatch_t m[2];
  const char *ptr = body;
  int local = 0;
  while (local < 500 && *count < MAX_SUBDOMAINS &&
         regexec(&re, ptr, 2, m, 0) == 0) {
    int len = (int)(m[0].rm_eo - m[0].rm_so);
    if (len > 0 && len < 255) {
      char sd[256];
      memcpy(sd, ptr + m[0].rm_so, len);
      sd[len] = '\0';
      if (strstr(sd, domain) && strlen(sd) > strlen(domain) &&
          !already(results, *count, sd)) {
        strcpy(results[*count].subdomain, sd);
        strncpy(results[*count].source, src->name, sizeof results[*count].source - 1);
        results[*count].source[sizeof results[*count].source - 1] = '\0';
        (*count)++;
        local++;
      }
    }
    if (m[0].rm_eo <= m[0].rm_so) break;   /* avoid zero-width infinite loop */
    ptr += m[0].rm_eo;
  }
  regfree(&re);
  free(body);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;

  sub_t *results = calloc(MAX_SUBDOMAINS, sizeof(sub_t));
  if (!results) return -1;
  int count = 0;

  /* Phase 1: Certificate Transparency */
  count += search_crtsh(ctx->http, domain, results, MAX_SUBDOMAINS);

  /* Phase 2: all other sources (sequential — same result set as the 10-thread
   * split upstream; the threading only divides the source list). */
  int nsrc = (int)(sizeof search_sources / sizeof search_sources[0]);
  for (int i = 0; i < nsrc && count < MAX_SUBDOMAINS; i++)
    search_source(ctx->http, &search_sources[i], domain, results, &count);

  time_t start_time = time(NULL);
  cJSON *results_arr = cJSON_CreateArray();
  int total = 0, found_count = 0;
  for (int i = 0; i < count && i < 500; i++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "source", results[i].source);
    cJSON_AddBoolToObject(o, "found", 1);
    cJSON_AddNumberToObject(o, "confidence", CONFIDENCE_HIGH);
    cJSON_AddStringToObject(o, "detection_method", DETECTION_DIRECT);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "subdomain", results[i].subdomain);
    cJSON_AddStringToObject(data, "source", results[i].source);
    cJSON_AddBoolToObject(data, "verified", 0);
    cJSON_AddItemToObject(o, "data", data);
    cJSON_AddNullToObject(o, "llama_analysis");
    cJSON_AddItemToArray(results_arr, o);
    total++; found_count++;
  }
  free(results);

  /* result_builder_finalize envelope */
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", domain);
  cJSON_AddStringToObject(root, "query_type", "domain");
  cJSON_AddItemToObject(root, "results", results_arr);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", found_count);
  cJSON *breakdown = cJSON_CreateObject();
  cJSON_AddNumberToObject(breakdown, "direct", total);
  cJSON_AddNumberToObject(breakdown, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", breakdown);
  cJSON_AddItemToObject(root, "summary", summary);
  cJSON *meta = cJSON_CreateObject();
  time_t end_time = time(NULL);
  cJSON_AddNumberToObject(meta, "execution_time_ms", (int)((end_time - start_time) * 1000));
  cJSON_AddBoolToObject(meta, "llama_fallback_used", 0);
  cJSON_AddNumberToObject(meta, "timestamp", (double)end_time);
  cJSON_AddItemToObject(root, "_meta", meta);

  int confidence = total > 0 ? 90 : 60;

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SUBDOMAIN_FINDER");
  cJSON_AddStringToObject(props, "entity", domain);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", confidence);
  cJSON_AddNumberToObject(props, "subdomains_found", total);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "subdomains:%s", domain);
  char title[320];
  snprintf(title, sizeof title, "SUBDOMAIN_FINDER — %s", domain);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = total > 0 ? "subdomains found" : "no subdomains found";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SUBDOMAIN_FINDER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def subdomain_osint_def = {
  .id = "SUBDOMAIN_FINDER", .collector = "osint",
  .name = "Subdomain Finder", .name_ja = "サブドメイン検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(subdomain_osint_def)
