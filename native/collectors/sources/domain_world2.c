/* collectors/sources/domain_world2.c
 * OSINT services — key-gated host/domain/IP enrichment, pivot on ctx->entity.
 * Honest-empty when the key is unset.
 *
 *   HOST_IO_FULL     host.io/api/full/<domain>?token=          HOSTIO_TOKEN
 *   WHOISFREAKS      api.whoisfreaks.com/v1.0/whois?...         WHOISFREAKS_API_KEY
 *   IPINFO_LOOKUP    ipinfo.io/<ip>?token=                      IPINFO_TOKEN */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *DM_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* host.io/api/full/<domain> → {domain, web:{}, dns:{}, ipinfo:{}, related:{}} */
static int run_hostio(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("HOSTIO_TOKEN");
  if (!k) { fprintf(stderr, "[HOST_IO_FULL] no HOSTIO_TOKEN\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://host.io/api/full/%s?token=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", DM_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "HOST_IO_FULL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *domain = jo_sv(root, "domain");
  if (!domain) { cJSON_Delete(root); return 0; }
  const cJSON *web = cJSON_GetObjectItem(root, "web");
  const cJSON *dns = cJSON_GetObjectItem(root, "dns");
  const char *rank = web ? jo_sv(web, "rank") : NULL;
  const char *title_w = web ? jo_sv(web, "title") : NULL;
  const char *ip = dns ? jo_sv(dns, "a") : NULL;   /* may be array; jo_sv → NULL if not string */
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "domain", domain);
  if (title_w) cJSON_AddStringToObject(d, "web_title", title_w);
  if (rank) cJSON_AddStringToObject(d, "rank", rank);
  if (ip) cJSON_AddStringToObject(d, "a_record", ip);
  cJSON_AddStringToObject(d, "source", "host.io");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "HOST_IO_FULL");
  cJSON_AddStringToObject(p, "domain", domain);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char link[300]; snprintf(link, sizeof link, "https://host.io/%s", domain);
  intel_item it = {0};
  it.remote_key = domain; it.title = domain; it.summary = title_w;
  it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "domain-profile";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"HOST_IO_FULL\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_whoisfreaks(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("WHOISFREAKS_API_KEY");
  if (!k) { fprintf(stderr, "[WHOISFREAKS] no WHOISFREAKS_API_KEY\n"); return 0; }
  char url[640];
  snprintf(url, sizeof url,
    "https://api.whoisfreaks.com/v1.0/whois?apiKey=%s&whois=live&domainName=%s", k, enc);
  const char *hdrs[] = { "Accept: application/json", DM_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "WHOISFREAKS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *dn = jo_sv(root, "domain_name");
  if (!dn) { cJSON_Delete(root); return 0; }
  const char *created = jo_sv(root, "create_date");
  const char *expiry = jo_sv(root, "expiry_date");
  const cJSON *reg = cJSON_GetObjectItem(root, "domain_registrar");
  const char *registrar = reg ? jo_sv(reg, "registrar_name") : jo_sv(root, "registrar_name");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "domain_name", dn);
  if (created) cJSON_AddStringToObject(d, "create_date", created);
  if (expiry) cJSON_AddStringToObject(d, "expiry_date", expiry);
  if (registrar) cJSON_AddStringToObject(d, "registrar", registrar);
  cJSON_AddStringToObject(d, "source", "WhoisFreaks");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "WHOISFREAKS");
  cJSON_AddStringToObject(p, "domain", dn);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  intel_item it = {0};
  it.remote_key = dn; it.title = dn; it.summary = registrar;
  it.body = bj; it.lang = "en"; it.published_at = created;
  it.record_type = "whois-record";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"WHOISFREAKS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_ipinfo(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("IPINFO_TOKEN");
  if (!k) { fprintf(stderr, "[IPINFO_LOOKUP] no IPINFO_TOKEN\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://ipinfo.io/%s?token=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", DM_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "IPINFO_LOOKUP");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *ip = jo_sv(root, "ip");
  if (!ip) { cJSON_Delete(root); return 0; }
  const char *org = jo_sv(root, "org");
  const char *city = jo_sv(root, "city");
  const char *region = jo_sv(root, "region");
  const char *country = jo_sv(root, "country");
  const char *loc = jo_sv(root, "loc");
  const char *hostname = jo_sv(root, "hostname");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "ip", ip);
  if (org) cJSON_AddStringToObject(d, "org", org);
  if (hostname) cJSON_AddStringToObject(d, "hostname", hostname);
  if (city) cJSON_AddStringToObject(d, "city", city);
  if (region) cJSON_AddStringToObject(d, "region", region);
  if (country) cJSON_AddStringToObject(d, "country", country);
  cJSON_AddStringToObject(d, "source", "IPinfo");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "IPINFO_LOOKUP");
  cJSON_AddStringToObject(p, "ip", ip);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  int has_geo = 0; double lat = 0, lon = 0;
  if (loc && strchr(loc, ',')) { lat = atof(loc); lon = atof(strchr(loc, ',') + 1); has_geo = 1; }
  char title[256];
  snprintf(title, sizeof title, "%s%s%s", ip, org ? " · " : "", org ? org : "");
  intel_item it = {0};
  it.remote_key = ip; it.title = title; it.summary = org;
  it.body = bj; it.lang = "en";
  it.has_geo = has_geo; it.lat = lat; it.lon = lon;
  it.record_type = "ip-geolocation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"IPINFO_LOOKUP\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "HOST_IO_FULL"))  n = run_hostio(ctx, sink, enc);
  else if (!strcmp(sid, "WHOISFREAKS"))   n = run_whoisfreaks(ctx, sink, enc);
  else if (!strcmp(sid, "IPINFO_LOOKUP")) n = run_ipinfo(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define DM_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/domain", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

DM_DEF(dm_hostio_def, "HOST_IO_FULL", "host.io Domain Full",
  "host.io (HOSTIO_TOKEN): domain web/dns/ipinfo/related profile.");
DM_DEF(dm_whoisfreaks_def, "WHOISFREAKS", "WhoisFreaks WHOIS",
  "WhoisFreaks (WHOISFREAKS_API_KEY): live WHOIS (dates, registrar).");
DM_DEF(dm_ipinfo_def, "IPINFO_LOOKUP", "IPinfo IP Lookup",
  "IPinfo (IPINFO_TOKEN): IP org/hostname/geo/ASN enrichment.");
