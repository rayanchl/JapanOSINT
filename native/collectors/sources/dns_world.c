/* collectors/sources/dns_world.c
 * OSINT services — DNS + IP whois, keyless and LIVE.
 *
 *   DOH_RESOLVE  On-demand domain pivot (ctx->entity = a hostname). Resolves the
 *                name over DNS-over-HTTPS (RFC 8484 JSON API) at dns.google and,
 *                as a fallback, cloudflare-dns.com, for A / AAAA / MX / TXT / NS
 *                record types. Emits one intel_item per real answer record parsed
 *                from the DoH JSON ("Answer" array). No answers / fetch failure →
 *                honest empty (return 0).
 *
 *   RDAP_IP      On-demand IP pivot (ctx->entity = an IPv4/IPv6 address). Queries
 *                the ARIN RDAP bootstrap (rdap.arin.net/registry/ip/<ip>), which
 *                301-redirects to the authoritative RIR, and emits the netblock:
 *                CIDR range, registered org/name, country, handle. One item per
 *                allocation entry. Non-IP entity / no data → honest empty.
 *
 * Both endpoints are public, keyless and return live parsed data only — nothing
 * is synthesized. One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ------------------------------------------------------------------ *
 * DoH: DNS-over-HTTPS JSON resolution                                 *
 * ------------------------------------------------------------------ */

/* Map a DoH numeric TYPE to a mnemonic for the record; the DoH JSON gives the
 * type as an integer in each Answer entry. */
static const char *dw_type_name(int t) {
  switch (t) {
    case 1:  return "A";
    case 2:  return "NS";
    case 5:  return "CNAME";
    case 15: return "MX";
    case 16: return "TXT";
    case 28: return "AAAA";
    case 6:  return "SOA";
    case 33: return "SRV";
    case 257:return "CAA";
    default: return "OTHER";
  }
}

/* Emit one resolved DNS answer record. All fields are parsed from the live DoH
 * response. Returns 1 on emit. */
static int dw_emit_answer(intel_sink *sink, const char *domain,
                          const char *qtype, const cJSON *ans, int idx) {
  const char *name = jo_sv(ans, "name");
  const char *rdata = jo_sv(ans, "data");
  if (!rdata) return 0;
  const cJSON *tj = cJSON_GetObjectItem(ans, "type");
  int tnum = (tj && cJSON_IsNumber(tj)) ? tj->valueint : -1;
  const cJSON *ttlj = cJSON_GetObjectItem(ans, "TTL");
  int ttl = (ttlj && cJSON_IsNumber(ttlj)) ? ttlj->valueint : -1;
  const char *rtype = (tnum >= 0) ? dw_type_name(tnum) : qtype;

  cJSON *data = cJSON_CreateObject();
  if (name)  cJSON_AddStringToObject(data, "name", name);
  cJSON_AddStringToObject(data, "record_type", rtype);
  cJSON_AddStringToObject(data, "value", rdata);
  if (ttl >= 0) cJSON_AddNumberToObject(data, "ttl", ttl);
  cJSON_AddStringToObject(data, "source", "DNS-over-HTTPS");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DOH_RESOLVE");
  cJSON_AddStringToObject(props, "domain", domain);
  cJSON_AddStringToObject(props, "dns_type", rtype);
  cJSON_AddStringToObject(props, "value", rdata);
  if (ttl >= 0) cJSON_AddNumberToObject(props, "ttl", ttl);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[512];
  snprintf(title, sizeof title, "%s %s %s", domain, rtype, rdata);
  char key[600];
  snprintf(key, sizeof key, "%s|%s|%d|%.400s", domain, rtype, idx, rdata);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = rdata;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "dns-record";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"dns\",\"doh\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Fetch one DoH query (name+type) and emit its Answer records. Returns count. */
static int dw_doh_query(const source_ctx *ctx, intel_sink *sink,
                        const char *base, const char *domain, const char *type) {
  char *enc = jo_urlencode(domain);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "%s?name=%s&type=%s", base, enc, type);
  free(enc);
  /* application/dns-json is the DoH JSON content type (dns.google & cloudflare). */
  const char *hdrs[] = {
    "Accept: application/dns-json",
    "User-Agent: JapanOSINT/1.0 (dns-resolve)",
    NULL };
  char *body = jo_get(ctx, url, hdrs, "DOH_RESOLVE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0, idx = 0;
  cJSON *answ = cJSON_GetObjectItem(root, "Answer");
  if (cJSON_IsArray(answ)) {
    cJSON *a;
    cJSON_ArrayForEach(a, answ) {
      emitted += dw_emit_answer(sink, domain, type, a, idx);
      idx++;
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int dw_run_doh(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!domain) return -1;
  /* reject obvious non-hostnames (must contain a dot, no spaces/@). */
  if (strchr(domain, ' ') || strchr(domain, '@') || !strchr(domain, '.')) {
    fprintf(stderr, "[DOH_RESOLVE] entity not a hostname, skipping\n");
    return 0;
  }

  static const char *TYPES[] = { "A", "AAAA", "MX", "TXT", "NS" };
  static const char *PRIMARY  = "https://dns.google/resolve";
  static const char *FALLBACK = "https://cloudflare-dns.com/dns-query";

  int total = 0;
  for (size_t i = 0; i < sizeof TYPES / sizeof TYPES[0]; i++) {
    int got = dw_doh_query(ctx, sink, PRIMARY, domain, TYPES[i]);
    if (got == 0)  /* try the other resolver before giving up on this type */
      got = dw_doh_query(ctx, sink, FALLBACK, domain, TYPES[i]);
    total += got;
  }
  fprintf(stderr, "[DOH_RESOLVE] emitted %d\n", total);
  return 0;   /* honest empty is not an error */
}

/* ------------------------------------------------------------------ *
 * RDAP: IP → netblock / org                                           *
 * ------------------------------------------------------------------ */

/* Does the entity look like an IPv4 or IPv6 literal? */
static int dw_is_ip(const char *s) {
  if (!s || !*s) return 0;
  int dots = 0, colons = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '.') dots++;
    else if (*p == ':') colons++;
    else if (!isxdigit((unsigned char)*p)) return 0;
  }
  return (dots == 3 && colons == 0) || (colons >= 2);
}

/* Pull the registrant/registrant-org name from an RDAP "entities" array. Returns
 * a malloc'd string (caller frees) or NULL. RDAP encodes contact data as jCard
 * (vcardArray). We look for the "fn" (formatted name) property. */
static char *dw_rdap_org(const cJSON *root) {
  const cJSON *ents = cJSON_GetObjectItem(root, "entities");
  if (!cJSON_IsArray(ents)) return NULL;
  const cJSON *e;
  cJSON_ArrayForEach(e, ents) {
    const cJSON *vc = cJSON_GetObjectItem(e, "vcardArray");
    /* vcardArray = [ "vcard", [ [ "fn", {}, "text", "Org Name" ], ... ] ] */
    if (cJSON_IsArray(vc) && cJSON_GetArraySize(vc) >= 2) {
      const cJSON *props = cJSON_GetArrayItem(vc, 1);
      const cJSON *prop;
      cJSON_ArrayForEach(prop, props) {
        if (cJSON_IsArray(prop) && cJSON_GetArraySize(prop) >= 4) {
          const cJSON *pname = cJSON_GetArrayItem(prop, 0);  /* exhaustive-ok: jCard property is [name,params,type,value] */
          const cJSON *pval  = cJSON_GetArrayItem(prop, 3);
          if (pname && cJSON_IsString(pname) && pval && cJSON_IsString(pval) &&
              strcmp(pname->valuestring, "fn") == 0 && pval->valuestring[0]) {
            return strdup(pval->valuestring);
          }
        }
      }
    }
    /* fall back to the entity handle if no fn found */
    const char *h = jo_sv(e, "handle");
    if (h) return strdup(h);
  }
  return NULL;
}

static int dw_run_rdap(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!ip) return -1;
  if (!dw_is_ip(ip)) {
    fprintf(stderr, "[RDAP_IP] entity not an IP, skipping\n");
    return 0;
  }

  char *enc = jo_urlencode(ip);
  if (!enc) return 0;
  char url[512];
  /* ARIN RDAP redirects to the owning RIR; jo_get follows redirects (curl -L). */
  snprintf(url, sizeof url, "https://rdap.arin.net/registry/ip/%s", enc);
  free(enc);

  const char *hdrs[] = {
    "Accept: application/rdap+json, application/json",
    "User-Agent: JapanOSINT/1.0 (rdap-ip)",
    NULL };
  char *body = jo_get(ctx, url, hdrs, "RDAP_IP");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[RDAP_IP] parse fail\n"); return 0; }

  const char *handle    = jo_sv(root, "handle");
  const char *name      = jo_sv(root, "name");
  const char *start     = jo_sv(root, "startAddress");
  const char *end       = jo_sv(root, "endAddress");
  const char *country   = jo_sv(root, "country");
  const char *iptype    = jo_sv(root, "type");
  /* CIDR blocks, if present */
  char cidr[128] = {0};
  const cJSON *cidrs = cJSON_GetObjectItem(root, "cidr0_cidrs");
  if (cJSON_IsArray(cidrs) && cJSON_GetArraySize(cidrs) > 0) {
    const cJSON *c0 = cJSON_GetArrayItem(cidrs, 0);  /* exhaustive-ok: display pick; cidrs_all carries every value */
    const char *v4 = jo_sv(c0, "v4prefix");
    const char *v6 = jo_sv(c0, "v6prefix");
    const cJSON *len = cJSON_GetObjectItem(c0, "length");
    if ((v4 || v6) && len && cJSON_IsNumber(len))
      snprintf(cidr, sizeof cidr, "%s/%d", v4 ? v4 : v6, len->valueint);
  }
  char *org = dw_rdap_org(root);

  if (!handle && !name && !start && !org) {
    cJSON_Delete(root);
    free(org);
    fprintf(stderr, "[RDAP_IP] emitted 0\n");
    return 0;
  }

  char range[160] = {0};
  if (cidr[0]) snprintf(range, sizeof range, "%s", cidr);
  else if (start && end) snprintf(range, sizeof range, "%s - %s", start, end);
  else if (start) snprintf(range, sizeof range, "%s", start);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  /* An allocation can list several prefixes; `range` shows one, so keep the
   * whole list too (docs/SOURCE_EXHAUSTIVENESS.md). */
  if (cJSON_IsArray(cidrs) && cJSON_GetArraySize(cidrs) > 0)
    cJSON_AddItemToObject(data, "cidrs_all", cJSON_Duplicate(cidrs, 1));
  if (range[0]) cJSON_AddStringToObject(data, "range", range);
  if (handle)   cJSON_AddStringToObject(data, "handle", handle);
  if (name)     cJSON_AddStringToObject(data, "netname", name);
  if (org)      cJSON_AddStringToObject(data, "org", org);
  if (country)  cJSON_AddStringToObject(data, "country", country);
  if (iptype)   cJSON_AddStringToObject(data, "ip_version", iptype);
  cJSON_AddStringToObject(data, "source", "RDAP");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "RDAP_IP");
  cJSON_AddStringToObject(props, "ip", ip);
  if (range[0]) cJSON_AddStringToObject(props, "range", range);
  if (org)      cJSON_AddStringToObject(props, "org", org);
  if (country)  cJSON_AddStringToObject(props, "country", country);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[512];
  snprintf(title, sizeof title, "%s%s%s%s", ip,
           range[0] ? " (" : "", range[0] ? range : "", range[0] ? ")" : "");
  char key[256];
  snprintf(key, sizeof key, "RDAP_IP|%s", handle ? handle : ip);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = org ? org : name;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "ip-whois";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"rdap\",\"whois\"]";
  sink->emit(sink, &it);
  free(bj); free(pj); free(org);
  cJSON_Delete(root);
  fprintf(stderr, "[RDAP_IP] emitted 1\n");
  return 0;
}

/* ------------------------------------------------------------------ *
 * dispatch                                                            *
 * ------------------------------------------------------------------ */
static int run(const source_ctx *ctx, intel_sink *sink) {
  if (strcmp(ctx->source_id, "DOH_RESOLVE") == 0) return dw_run_doh(ctx, sink);
  if (strcmp(ctx->source_id, "RDAP_IP") == 0)     return dw_run_rdap(ctx, sink);
  return -1;
}

static const source_def dns_doh_def = {
  .id = "DOH_RESOLVE", .collector = "osint",
  .name = "DNS-over-HTTPS Resolver", .name_ja = "DNS-over-HTTPS 名前解決",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://dns.google/resolve",
  .description = "Resolve a hostname's A/AAAA/MX/TXT/NS records over DNS-over-HTTPS (dns.google, cloudflare-dns), keyless LIVE",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dns_doh_def)

static const source_def dns_rdap_def = {
  .id = "RDAP_IP", .collector = "osint",
  .name = "RDAP IP Whois", .name_ja = "RDAP IP Whois",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://rdap.arin.net/registry/ip/",
  .description = "RDAP netblock/org lookup for an IP address via the ARIN bootstrap (redirects to owning RIR), keyless LIVE",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dns_rdap_def)
