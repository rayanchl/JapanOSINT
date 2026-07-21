/* collectors/osint/sources/reverse_whois.c
 * OSINT service — faithful port of OSINTsaas osint_tools/reverse_whois.c
 * (handle_reverse_whois). Canonical SERVICE name in osint_dispatcher.c
 * service_registry[] bound to handle_reverse_whois is "HISTORICAL_WHOIS"
 * (line 218). On-demand (interval 0); dispatcher runs it with ctx->entity =
 * a registrant name / organization / email (single entity → upstream entity
 * loop runs once).
 *
 * Reproduces handle_reverse_whois() faithfully: ViewDNS reverse-whois HTML is
 * scraped for domains in the first <td> of each <tr> (always free, no key);
 * when WHOISXML_API_KEY is set a JSON POST to reverse-whois.whoisxmlapi.com
 * (email vs name/org search terms) yields domainsList[]; when
 * SECURITYTRAILS_API_KEY is set a GET to api.securitytrails.com/v1/domains/
 * list yields records[].hostname (upstream issued this GET unauthenticated —
 * header support was a TODO — preserved verbatim). Domains are case-
 * insensitively deduplicated. Builds the identical root: { service:
 * "REVERSE_WHOIS", api_keys_configured, total_domains_found,
 * results:[{query, domains_found, domains:[{domain,source}]}],
 * sources_queried:[…] }. Upstream keeps service_name "REVERSE_WHOIS" in the
 * data even though the dispatcher's canonical name is HISTORICAL_WHOIS —
 * preserved. Free ViewDNS path means no key gate.
 *
 * PER-RECORD EMIT: emits ONE intel_item per associated DOMAIN
 * (remote_key="domain:<domain>"), body {domain,source}, deduped case-
 * insensitively within the run. If no domains are found, emits nothing and
 * returns 0 (honest empty). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

static char *url_encode_dup(const char *in) {
  size_t n = strlen(in);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[w++] = (char)c;
    } else {
      sprintf(out + w, "%%%02X", c);
      w += 3;
    }
  }
  out[w] = 0;
  return out;
}

/* ViewDNS: scrape <td>…domain…</td> from each <tr> inside the first <table> */
static void query_viewdns_reverse(http_client *http, const char *query, cJSON *combined) {
  char *enc = url_encode_dup(query);
  if (!enc) return;
  char url[512];
  snprintf(url, sizeof url, "https://viewdns.info/reversewhois/?q=%s", enc);
  free(enc);

  char *body = feed_get_text(http, url, 30000);
  if (!body) return;

  char *table_start = strstr(body, "<table");
  if (table_start) {
    char *pos = table_start;
    while ((pos = strstr(pos, "<tr>")) != NULL) {
      char *td = strstr(pos, "<td>");
      if (td && td < pos + 500) {
        td += 4;
        char *td_end = strstr(td, "</td>");
        if (td_end) {
          size_t len = (size_t)(td_end - td);
          if (len > 0 && len < 256) {
            char domain[256];
            memcpy(domain, td, len);
            domain[len] = '\0';
            if (strchr(domain, '.') && !strchr(domain, '<')) {
              cJSON *item = cJSON_CreateObject();
              cJSON_AddStringToObject(item, "domain", domain);
              cJSON_AddStringToObject(item, "source", "viewdns");
              cJSON_AddItemToArray(combined, item);
            }
          }
        }
      }
      pos++;
    }
  }
  free(body);
}

/* WhoisXML reverse-whois POST (requires key); email vs name/org search terms */
static void query_whoisxml_reverse(http_client *http, const char *query,
                                   const char *api_key, cJSON *combined) {
  if (!api_key || !*api_key) return;

  cJSON *request = cJSON_CreateObject();
  cJSON_AddStringToObject(request, "apiKey", api_key);
  if (strchr(query, '@')) {
    cJSON *search = cJSON_CreateObject();
    cJSON_AddStringToObject(search, "include", query);
    cJSON *basic = cJSON_CreateArray();
    cJSON_AddItemToArray(basic, cJSON_CreateString("RegistrantContact.Email"));
    cJSON_AddItemToObject(search, "basicSearchTerms", basic);
    cJSON_AddItemToObject(request, "searchType", cJSON_CreateString("current"));
    cJSON_AddItemToObject(request, "mode", cJSON_CreateString("purchase"));
    cJSON_AddItemToObject(request, "basicSearchTerms", search);
  } else {
    cJSON *search = cJSON_CreateObject();
    cJSON_AddStringToObject(search, "include", query);
    cJSON *basic = cJSON_CreateArray();
    cJSON_AddItemToArray(basic, cJSON_CreateString("RegistrantContact.Name"));
    cJSON_AddItemToArray(basic, cJSON_CreateString("RegistrantContact.Organization"));
    cJSON_AddItemToObject(search, "basicSearchTerms", basic);
    cJSON_AddItemToObject(request, "searchType", cJSON_CreateString("current"));
    cJSON_AddItemToObject(request, "basicSearchTerms", search);
  }
  char *post_data = cJSON_PrintUnformatted(request);
  cJSON_Delete(request);

  const char *headers[] = { "Content-Type: application/json", NULL };
  cJSON *json = feed_post_json(http, "https://reverse-whois.whoisxmlapi.com/api/v2",
                               post_data, headers, 30000);
  free(post_data);
  if (json) {
    cJSON *domains_list = cJSON_GetObjectItem(json, "domainsList");
    if (domains_list && cJSON_IsArray(domains_list)) {
      int count = cJSON_GetArraySize(domains_list);
      for (int i = 0; i < count && i < 100; i++) {
        cJSON *d = cJSON_GetArrayItem(domains_list, i);
        if (d && d->valuestring) {
          cJSON *item = cJSON_CreateObject();
          cJSON_AddStringToObject(item, "domain", d->valuestring);
          cJSON_AddStringToObject(item, "source", "whoisxml");
          cJSON_AddItemToArray(combined, item);
        }
      }
    }
    cJSON_Delete(json);
  }
}

/* SecurityTrails GET (upstream issued this unauthenticated — header was TODO) */
static void query_securitytrails_reverse(http_client *http, const char *query,
                                         const char *api_key, cJSON *combined) {
  if (!api_key || !*api_key) return;
  const char *endpoint = strchr(query, '@')
    ? "/v1/domains/list?include_ips=false&whois_email=%s"
    : "/v1/domains/list?include_ips=false&whois_organization=%s";

  char *enc = url_encode_dup(query);
  if (!enc) return;
  char tmpl[512];
  snprintf(tmpl, sizeof tmpl, "https://api.securitytrails.com%s", endpoint);
  char full_url[1024];
  snprintf(full_url, sizeof full_url, tmpl, enc);
  free(enc);

  cJSON *json = feed_get_json(http, full_url, 30000);
  if (json) {
    cJSON *records = cJSON_GetObjectItem(json, "records");
    if (records && cJSON_IsArray(records)) {
      int count = cJSON_GetArraySize(records);
      for (int i = 0; i < count && i < 100; i++) {
        cJSON *rec = cJSON_GetArrayItem(records, i);
        if (rec) {
          cJSON *hostname = cJSON_GetObjectItem(rec, "hostname");
          if (hostname && hostname->valuestring) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "domain", hostname->valuestring);
            cJSON_AddStringToObject(item, "source", "securitytrails");
            cJSON_AddItemToArray(combined, item);
          }
        }
      }
    }
    cJSON_Delete(json);
  }
}

/* Emit ONE intel_item for a single associated domain. Returns 1 if emitted. */
static int emit_domain(intel_sink *sink, const char *domain, const char *source) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "domain", domain);
  cJSON_AddStringToObject(data, "source", source ? source : "viewdns");
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "HISTORICAL_WHOIS");
  cJSON_AddStringToObject(props, "source", source ? source : "viewdns");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "domain:%s", domain);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = domain;
  it.body            = bj;
  it.summary         = source ? source : "viewdns";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"HISTORICAL_WHOIS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  const char *whoisxml_key = getenv("WHOISXML_API_KEY");
  const char *securitytrails_key = getenv("SECURITYTRAILS_API_KEY");

  cJSON *combined = cJSON_CreateArray();
  query_viewdns_reverse(ctx->http, entity, combined);
  if (whoisxml_key && *whoisxml_key)
    query_whoisxml_reverse(ctx->http, entity, whoisxml_key, combined);
  if (securitytrails_key && *securitytrails_key)
    query_securitytrails_reverse(ctx->http, entity, securitytrails_key, combined);

  /* case-insensitive dedup by domain (upstream deduplicate_domains); emit one
   * intel_item per unique domain. */
  const char *seen[1000];
  int seen_count = 0, emitted = 0;
  cJSON *item;
  cJSON_ArrayForEach(item, combined) {
    cJSON *d = cJSON_GetObjectItem(item, "domain");
    if (!d || !d->valuestring) continue;
    int found = 0;
    for (int i = 0; i < seen_count; i++)
      if (strcasecmp(seen[i], d->valuestring) == 0) { found = 1; break; }
    if (found || seen_count >= 1000) continue;
    seen[seen_count++] = d->valuestring;
    cJSON *src = cJSON_GetObjectItem(item, "source");
    emitted += emit_domain(sink, d->valuestring,
                           (src && src->valuestring) ? src->valuestring : NULL);
  }
  cJSON_Delete(combined);

  (void)emitted;
  return 0;                  /* no domains → honest empty, not an error */
}

static const source_def reverse_whois_def = {
  .id = "HISTORICAL_WHOIS", .collector = "osint",
  .name = "Historical WHOIS", .name_ja = "過去のWHOIS",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/historical-whois",
  .description = "Find domains by registrant via ViewDNS, WhoisXML, SecurityTrails.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reverse_whois_def)
