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
 * preserved. Free ViewDNS path means no key gate. success=true,
 * confidence 80. Emits one osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  const char *whoisxml_key = getenv("WHOISXML_API_KEY");
  const char *securitytrails_key = getenv("SECURITYTRAILS_API_KEY");
  int has_keys = (whoisxml_key && *whoisxml_key) ||
                 (securitytrails_key && *securitytrails_key);

  cJSON *root = cJSON_CreateObject();
  cJSON *all_results = cJSON_CreateArray();
  int total_domains = 0;

  /* dispatcher → single entity; upstream entity loop runs once */
  cJSON *er = cJSON_CreateObject();
  cJSON_AddStringToObject(er, "query", entity);
  cJSON *combined = cJSON_CreateArray();

  query_viewdns_reverse(ctx->http, entity, combined);
  if (whoisxml_key && *whoisxml_key)
    query_whoisxml_reverse(ctx->http, entity, whoisxml_key, combined);
  if (securitytrails_key && *securitytrails_key)
    query_securitytrails_reverse(ctx->http, entity, securitytrails_key, combined);

  /* case-insensitive dedup by domain (upstream deduplicate_domains) */
  cJSON *unique = cJSON_CreateArray();
  const char *seen[1000];
  int seen_count = 0;
  cJSON *item;
  cJSON_ArrayForEach(item, combined) {
    cJSON *d = cJSON_GetObjectItem(item, "domain");
    if (!d || !d->valuestring) continue;
    int found = 0;
    for (int i = 0; i < seen_count; i++)
      if (strcasecmp(seen[i], d->valuestring) == 0) { found = 1; break; }
    if (!found && seen_count < 1000) {
      seen[seen_count++] = d->valuestring;
      cJSON_AddItemToArray(unique, cJSON_Duplicate(item, 1));
    }
  }
  int domain_count = cJSON_GetArraySize(unique);
  total_domains += domain_count;
  cJSON_AddNumberToObject(er, "domains_found", domain_count);
  cJSON_AddItemToObject(er, "domains", unique);
  cJSON_Delete(combined);
  cJSON_AddItemToArray(all_results, er);

  cJSON_AddStringToObject(root, "service", "REVERSE_WHOIS");
  cJSON_AddBoolToObject(root, "api_keys_configured", has_keys);
  cJSON_AddNumberToObject(root, "total_domains_found", total_domains);
  cJSON_AddItemToObject(root, "results", all_results);
  cJSON *sources = cJSON_CreateArray();
  cJSON_AddItemToArray(sources, cJSON_CreateString("viewdns"));
  if (whoisxml_key && *whoisxml_key) cJSON_AddItemToArray(sources, cJSON_CreateString("whoisxml"));
  if (securitytrails_key && *securitytrails_key) cJSON_AddItemToArray(sources, cJSON_CreateString("securitytrails"));
  cJSON_AddItemToObject(root, "sources_queried", sources);

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "HISTORICAL_WHOIS");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "revwhois:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "HISTORICAL_WHOIS — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = domain_count > 0 ? "related domains found" : "no related domains";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"HISTORICAL_WHOIS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def reverse_whois_def = {
  .id = "HISTORICAL_WHOIS", .collector = "osint",
  .name = "Historical WHOIS", .name_ja = "過去のWHOIS",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(reverse_whois_def)
