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
#include "lib/jocore.h"
#include "lib/seenset.h"    /* growable dedup — no fixed cap on domains */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* Next <td ...> cell at or after *pos: copy its text into out, advance *pos
 * past the closing </td>. Returns 0 when no further cell exists in this row. */
static int next_cell(char **pos, const char *row_end, char *out, size_t cap) {
  char *td = strstr(*pos, "<td");
  if (!td || (row_end && td >= row_end)) return 0;
  char *gt = strchr(td, '>');                    /* skip the attribute list */
  if (!gt) return 0;
  char *s = gt + 1;
  char *e = strstr(s, "</td>");
  if (!e) return 0;
  size_t o = 0;
  for (char *c = s; c < e && o + 1 < cap; c++) {
    if (*c == '<') {                             /* drop any nested markup */
      char *ct = strchr(c, '>');
      if (!ct || ct >= e) break;
      c = ct; continue;
    }
    if ((unsigned char)*c <= ' ' && o == 0) continue;   /* left-trim */
    out[o++] = *c;
  }
  while (o && (unsigned char)out[o - 1] <= ' ') o--;    /* right-trim */
  out[o] = '\0';
  *pos = e + 5;
  return 1;
}

/* ViewDNS reverse-whois result table.
 *
 * AUDIT NOTE (slice a3, 2026-07-31): this is the ONLY key-free path of
 * HISTORICAL_WHOIS, and it had silently stopped matching. The old scraper
 * looked for the literal strings "<tr>" and "<td>"; ViewDNS has since moved to
 * Tailwind markup where every cell is
 *   <td class="px-6 py-4 whitespace-nowrap …">example.com</td>
 * so strstr(pos,"<td>") never hit and the source reported an honest-looking
 * empty on every lookup. Verified against the live page today: the result
 * table carries 125 rows for q=github.com. Cells are matched on "<td" plus the
 * attribute list now.
 *
 * The table has THREE columns — domain, registration date, registrar — and the
 * old code carried only the first. All three are kept now; the registration
 * date is what makes the row a *historical* whois result at all. */
static void query_viewdns_reverse(http_client *http, const char *query, cJSON *combined) {
  char *enc = jo_urlencode(query);
  if (!enc) return;
  char url[512];
  snprintf(url, sizeof url, "https://viewdns.info/reversewhois/?q=%s", enc);
  free(enc);

  char *body = feed_get_text(http, url, 30000);
  if (!body) return;

  char *table_start = strstr(body, "<table");
  int found = 0;
  if (table_start) {
    char *pos = table_start;
    while ((pos = strstr(pos, "<tr")) != NULL) {
      char *row_end = strstr(pos, "</tr>");
      pos = strchr(pos, '>');
      if (!pos) break;
      pos++;

      char domain[256] = {0}, regdate[128] = {0}, registrar[256] = {0};
      char *cur = pos;
      if (!next_cell(&cur, row_end, domain, sizeof domain)) { pos = row_end ? row_end + 5 : pos; continue; }
      next_cell(&cur, row_end, regdate, sizeof regdate);
      next_cell(&cur, row_end, registrar, sizeof registrar);

      /* header row and any junk cell are rejected by requiring a dotted label */
      if (domain[0] && strchr(domain, '.') && !strchr(domain, ' ')) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "domain", domain);
        if (regdate[0])   cJSON_AddStringToObject(item, "registered", regdate);
        if (registrar[0]) cJSON_AddStringToObject(item, "registrar", registrar);
        cJSON_AddStringToObject(item, "source", "viewdns");
        cJSON_AddItemToArray(combined, item);
        found++;
      }
      pos = row_end ? row_end + 5 : cur;
    }
  }
  if (!found)
    fprintf(stderr, "[HISTORICAL_WHOIS] viewdns: no result rows parsed from %s\n", url);
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

  char *enc = jo_urlencode(query);
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

/* Emit ONE intel_item for a single associated domain. Returns 1 if emitted.
 * `registered` / `registrar` come from the ViewDNS result table and are NULL
 * for the key-gated providers, which do not publish them. */
static int emit_domain(intel_sink *sink, const char *domain, const char *source,
                       const char *registered, const char *registrar) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "domain", domain);
  if (registered) cJSON_AddStringToObject(data, "registered", registered);
  if (registrar)  cJSON_AddStringToObject(data, "registrar", registrar);
  cJSON_AddStringToObject(data, "source", source ? source : "viewdns");
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "HISTORICAL_WHOIS");
  cJSON_AddStringToObject(props, "source", source ? source : "viewdns");
  if (registered) cJSON_AddStringToObject(props, "registered", registered);
  if (registrar)  cJSON_AddStringToObject(props, "registrar", registrar);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "domain:%s", domain);
  char summ[320];
  if (registrar && *registrar)
    snprintf(summ, sizeof summ, "%s — %s", source ? source : "viewdns", registrar);
  else
    snprintf(summ, sizeof summ, "%s", source ? source : "viewdns");

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = domain;
  it.body            = bj;
  it.summary         = summ;
  it.link            = NULL;
  it.published_at    = (registered && *registered) ? registered : NULL;
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
  seen_set seen = {0};      /* grows: a registrant with >1000 domains used to
                             * lose everything past the 1000th */
  int emitted = 0;
  cJSON *item;
  cJSON_ArrayForEach(item, combined) {
    cJSON *d = cJSON_GetObjectItem(item, "domain");
    if (!d || !d->valuestring) continue;
    char lower[320];
    snprintf(lower, sizeof lower, "%s", d->valuestring);
    for (char *lp = lower; *lp; lp++) *lp = (char)tolower((unsigned char)*lp);
    if (!seen_add(&seen, lower)) continue;
    cJSON *src  = cJSON_GetObjectItem(item, "source");
    cJSON *reg  = cJSON_GetObjectItem(item, "registered");
    cJSON *rar  = cJSON_GetObjectItem(item, "registrar");
    emitted += emit_domain(sink, d->valuestring,
                           (src && src->valuestring) ? src->valuestring : NULL,
                           (reg && reg->valuestring) ? reg->valuestring : NULL,
                           (rar && rar->valuestring) ? rar->valuestring : NULL);
  }
  seen_free(&seen);
  cJSON_Delete(combined);

  fprintf(stderr, "[HISTORICAL_WHOIS] emitted %d domain(s) for %s\n",
          emitted, entity);
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
